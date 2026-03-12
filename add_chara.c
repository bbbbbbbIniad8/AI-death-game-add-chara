#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>
#include "cJSON.h"

#define MAX_BUF 512

int cleanup(const char* src_path, FILE *f1, FILE *f2, FILE *f3) {
    if (f1) fclose(f1);
    if (f2) fclose(f2);
    if (f3) fclose(f3);
    int result1 = remove("./json.tmp");
    int result2 = remove(src_path);
    return result1 + result2;
}

int read_constant_ts(char* buf, size_t size, FILE* src){
    if (fgets(buf, size, src) == NULL){
        return 1;
    }
    return 0;
}

void change_CHARACTER_IDS(char* buf, FILE* dst, const char* CId){
    char s[255];
    sprintf(s, "  '%s',\n", CId);
    fputs(buf, dst);
    if (strcmp(buf, "export const CHARACTER_IDS = [\n") == 0){
        fputs(s, dst);
        printf(" - CHARACTER_IDSの変更が完了しました。\n");
    }
}

void change_AGENT_PERSONALITIES(char* buf, size_t size, FILE* dst, FILE* prompt){
    if (strcmp(buf, "export const AGENT_PERSONALITIES: AIPersonality[] = [\n") == 0){
        while (1){
            if (fgets(buf, size, prompt) == NULL){
                break;
            }
            fputs(buf, dst);
        }
        printf(" - AGENT_PERSONALITIESの変更が完了しました。\n");
    }
}

int read_json(const char* path, char** cId) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("ファイルが開けません");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *data = (char *)malloc(size + 1);
    fread(data, 1, size, fp);
    data[size] = '\0';
    fclose(fp);

    cJSON *json = cJSON_Parse(data);
    free(data);
    if (!json) {
        fprintf(stderr, "JSON構文エラー: [%s]\n", cJSON_GetErrorPtr());
        return 1;
    }

    FILE *tmp = fopen("./json.tmp", "wb");
    if (!tmp) {
        cJSON_Delete(json);
        return 1;
    }

    #define GET_STR(obj, key) (cJSON_IsString(cJSON_GetObjectItemCaseSensitive(obj, key)) ? \
                               cJSON_GetObjectItemCaseSensitive(obj, key)->valuestring : "unknown")

    *cId  = _strdup(GET_STR(json, "characterId"));
    char *name   = GET_STR(json, "name");
    char *app    = GET_STR(json, "appearance");
    char *prof   = GET_STR(json, "profile");
    char *desc   = GET_STR(json, "description");
    char *tone   = GET_STR(json, "tone");

    int si = 0, coop = 0, cunn = 0;
    cJSON *stats = cJSON_GetObjectItemCaseSensitive(json, "stats");
    if (cJSON_IsObject(stats)) {
        cJSON *item;
        item = cJSON_GetObjectItemCaseSensitive(stats, "survivalInstinct");
        if (cJSON_IsNumber(item)) si = item->valueint;
        
        item = cJSON_GetObjectItemCaseSensitive(stats, "cooperativeness");
        if (cJSON_IsNumber(item)) coop = item->valueint;

        item = cJSON_GetObjectItemCaseSensitive(stats, "cunningness");
        if (cJSON_IsNumber(item)) cunn = item->valueint;
    }

    cJSON *tierObj = cJSON_GetObjectItemCaseSensitive(json, "unlockTier");
    int tierVal = cJSON_IsNumber(tierObj) ? tierObj->valueint : 0;

    fprintf(tmp, "  {\n");
    fprintf(tmp, "    characterId: '%s',\n", *cId);
    fprintf(tmp, "    name: '%s',\n", name);
    fprintf(tmp, "    appearance: '%s',\n", app);
    fprintf(tmp, "    profile: '%s',\n", prof);
    fprintf(tmp, "    description: '%s',\n", desc);
    fprintf(tmp, "    tone: '%s',\n", tone);
    fprintf(tmp, "    stats: { survivalInstinct: %d, cooperativeness: %d, cunningness: %d },\n", si, coop, cunn);
    fprintf(tmp, "    unlockTier: %d,\n", tierVal);
    fprintf(tmp, "  },\n");

    fclose(tmp);
    cJSON_Delete(json);
    return 0;
}

int open_file(const char* src_path, const char* tmp_path, char* prompt_path, FILE** prompt, FILE** src, FILE **dst, char **CId){
    if (read_json(prompt_path, CId) == 0){
        printf(" - %sの読み込みに成功。\n", prompt_path);
        *prompt = fopen("./json.tmp", "rb");
    } else{
        return 1;
    }

    *src = fopen(src_path, "r");
    *dst = fopen(tmp_path, "w");
    if (!(*prompt)) {
        cleanup(src_path, *src, *dst, *prompt);
        return 2;
    }
    if (!(*src) || !(*dst)) {
        cleanup(src_path, *src, *dst, *prompt);
        return 3;
    }else{
        printf(" - %sの存在を確認。\n", src_path);
    }
    return 0;
}

int main_process(char* prompt_path, char** CId){
    const char *src_path = "./lib/constants.ts";
    const char *tmp_path = "./lib/constants.ts.tmp";
    FILE *prompt, *src, *dst;
    int result_code = open_file(src_path, tmp_path, prompt_path, &prompt, &src, &dst, CId);
    if (result_code==1){
        printf(" - エラー1: %sの読み込みに失敗しました。パスの場所にファイルがあるか確認してください。\n", prompt_path);
        return 1;
    }else if (result_code==2){
        printf(" - エラー2: 原因不明のエラーです。開発者に問い合わせてください。\n");
        return 1;
    }else if (result_code==3){
        printf(" - エラー3: lib/content.tsを確認できません。add_chara.exeファイルを[AI-DEATH-GAME]フォルダーの直下において実行し直してください。\n");
        return 1;
    };

    printf("\ncontent.tsの変更を開始します。\n\n");
    char buf[255];
    while (1){
        if (read_constant_ts(buf, sizeof(buf), src) != 0){
            break;
        }
        change_CHARACTER_IDS(buf, dst, *CId);
        change_AGENT_PERSONALITIES(buf, sizeof(buf), dst, prompt);        
    }
    
    printf("\n");
    if (cleanup(src_path, src, dst, prompt) == 0) {
        if (rename(tmp_path, src_path) == 0) {
            printf("完了: すべての変更が保存されました。");
        }else{
            perror("エラー: ファイルの置換に失敗しました");
        }
    }else{
        perror("エラー: 元ファイルの削除に失敗しました");
    }
    printf("\n\n");
    return 0;
}

int main()
{
    char input[255];
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    printf("\n追加したいキャラデータが格納されているjsonファイルのパスを入力してください。\n: ");
    scanf("%254s", input);
    printf("\n");

    char* CId;
    int result_code = main_process(input, &CId);
    free(CId);
    if (result_code == 0){
        return 0;
    }else{
        return 1;
    }
}