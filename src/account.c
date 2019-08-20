/*
  Vita Account Manager - Switch between multiple PSN/SEN accounts on a PS Vita or PS TV.
  Copyright (C) 2019  "windsurfer1122"
  https://github.com/windsurfer1122

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdlib.h>  // for malloc()
#include <vitasdk.h>

#include <main.h>
#include <account.h>
#include <file.h>
#include <history.h>

#include <debugScreen.h>
#define printf psvDebugScreenPrintf


struct Dir_Entry {
	size_t size;
	char *name;
};

const char *const accounts_folder = "accounts/";

enum {
	KEY_TYPE_INT=0,
	KEY_TYPE_STR=1,
	KEY_TYPE_BIN=2,
};

#define REG_BUFFER_DEFAULT_SIZE 256
#define STRING_BUFFER_DEFAULT_SIZE 1024

const char *const reg_config_np = "/CONFIG/NP";
const char *const reg_config_system = "/CONFIG/SYSTEM";
const char *const file_reg_config_np = "registry/CONFIG/NP/";
const char *const file_reg_config_system = "registry/CONFIG/SYSTEM/";
const char *const file_ext_bin = ".bin";
const char *const file_ext_txt = ".txt";
const int reg_id_username = 14;
const int reg_id_login_id = 258;
const int reg_id_lang = 271;
const int reg_id_country = 270;
const int reg_id_yob = 272;
const int reg_id_mob = 273;
const int reg_id_dob = 274;
const int reg_id_env = 260;

// values from os0:kd/registry.db0 and https://github.com/devnoname120/RegistryEditorMOD/blob/master/regs.c
struct Registry_Entry template_reg_user_entries[] = {
	{ reg_id_username, reg_config_system, file_reg_config_system, "username", KEY_TYPE_STR, 17, NULL, },
	{ reg_id_login_id, reg_config_np, file_reg_config_np, "login_id", KEY_TYPE_STR, 65, NULL, },
	{ 257, reg_config_np, file_reg_config_np, "account_id", KEY_TYPE_BIN, 8, NULL, },
	{ 259, reg_config_np, file_reg_config_np, "password", KEY_TYPE_STR, 31, NULL, },
	{ reg_id_lang, reg_config_np, file_reg_config_np, "lang", KEY_TYPE_STR, 6, NULL, },
	{ reg_id_country, reg_config_np, file_reg_config_np, "country", KEY_TYPE_STR, 3, NULL, },
	{ reg_id_yob, reg_config_np, file_reg_config_np, "yob", KEY_TYPE_INT, 4, NULL, },
	{ reg_id_mob, reg_config_np, file_reg_config_np, "mob", KEY_TYPE_INT, 4, NULL, },
	{ reg_id_dob, reg_config_np, file_reg_config_np, "dob", KEY_TYPE_INT, 4, NULL, },
	{ 275, reg_config_np, file_reg_config_np, "has_subaccount", KEY_TYPE_INT, 4, NULL, },
	{ 256, reg_config_np, file_reg_config_np, "enable_np", KEY_TYPE_INT, 4, NULL, },
	{ 269, reg_config_np, file_reg_config_np, "download_confirmed", KEY_TYPE_INT, 4, NULL, },
	{ reg_id_env, reg_config_np, file_reg_config_np, "env", KEY_TYPE_STR, 17, NULL, },
};

struct Registry_Data template_reg_user_data = {
	.count = sizeof(template_reg_user_entries) / sizeof(template_reg_user_entries[0]),
	.size = sizeof(template_reg_user_entries),
	.entries = template_reg_user_entries,
};

struct File_Entry template_file_user_entries[] = {
	{ "tm0:", "tm0/", "npdrm/act.dat", false, },
	{ "tm0:", "tm0/", "psmdrm/act.dat", false, },
	{ "ur0:user/00/np/", "ur0/np/", "myprofile.dat", false, },
	{ "ur0:user/00/trophy/data/sce_trop/", NULL, "TRPUSER.DAT", false, },
	{ "ur0:user/00/trophy/data/sce_trop/sce_pfs/", NULL, "files.db", false, },
	{ "ux0:", NULL, "id.dat", false, },
	{ "imc0:", NULL, "id.dat", false, },
	{ "uma0:", NULL, "id.dat", false, },
};

struct File_Data template_file_user_data = {
	.count = sizeof(template_file_user_entries) / sizeof(template_file_user_entries[0]),
	.size = sizeof(template_file_user_entries),
	.entries = template_file_user_entries,
};


void init_reg_data(struct Registry_Data *reg_data)
{
	int i;

	// Copy user template to new reg entries array
	reg_data->count = template_reg_user_data.count;
	reg_data->size = template_reg_user_data.size;
	reg_data->idx_username = template_reg_user_data.idx_username;
	reg_data->idx_login_id = template_reg_user_data.idx_login_id;
	reg_data->entries = (struct Registry_Entry *)malloc(template_reg_user_data.size);
	sceClibMemcpy((void *)(reg_data->entries), (void *)(template_reg_user_data.entries), template_reg_user_data.size);

	// Alloc memory for each key
	for (i = 0; i < template_reg_user_data.count; i++) {
		switch(reg_data->entries[i].key_type) {
			case KEY_TYPE_INT:
				if (reg_data->entries[i].key_size <= 0) {
					reg_data->entries[i].key_size = sizeof(int);
				}
				break;
			case KEY_TYPE_STR:
			case KEY_TYPE_BIN:
				if (reg_data->entries[i].key_size <= 0) {
					reg_data->entries[i].key_size = (REG_BUFFER_DEFAULT_SIZE);
				}
				break;
		}
		reg_data->entries[i].key_value = (void *)malloc(reg_data->entries[i].key_size);
		sceClibMemset(reg_data->entries[i].key_value, 0x00, reg_data->entries[i].key_size);
	}

	return;
}

void free_reg_data(struct Registry_Data *reg_data)
{
	int i;

	// Free memory for each key
	for (i = 0; i < reg_data->count; i++) {
		if (reg_data->entries[i].key_value != NULL) {
			free(reg_data->entries[i].key_value);
		}
	}

	// Copy user template to new reg entries array
	free(reg_data->entries);

	return;
}

void get_initial_reg_data(struct Registry_Data *reg_data)
{
	int i;
	int key_id;
	unsigned int user_number;
	char string[(STRING_BUFFER_DEFAULT_SIZE)];
	char *value;

	// set/get initial registry user data
	for (i = 0; i < reg_data->count; i++) {
		key_id = reg_data->entries[i].key_id;
		if (key_id == reg_id_username) {  // create dummy user name
			sceKernelGetRandomNumber((void *)(&user_number), sizeof(user_number));
			user_number %= 998;
			user_number++;
			sceClibSnprintf(string, (STRING_BUFFER_DEFAULT_SIZE), "user%03i", user_number);
			//
			value = (char *)(reg_data->entries[i].key_value);
			sceClibStrncpy(value, string, reg_data->entries[i].key_size);
			value[reg_data->entries[i].key_size] = '\0';
		} else if (key_id == reg_id_lang) {  // keep language
			value = (char *)(reg_data->entries[i].key_value);
			sceRegMgrGetKeyStr(reg_data->entries[i].key_path, reg_data->entries[i].key_name, value, reg_data->entries[i].key_size);
			if (value[0] == '\0') {  // fallback dummy
				sceClibStrncpy(value, "en", reg_data->entries[i].key_size);
			}
			value[reg_data->entries[i].key_size] = '\0';
		} else if (key_id == reg_id_country) {  // keep country
			value = (char *)(reg_data->entries[i].key_value);
			sceRegMgrGetKeyStr(reg_data->entries[i].key_path, reg_data->entries[i].key_name, value, reg_data->entries[i].key_size);
			if (value[0] == '\0') {  // fallback dummy
				sceClibStrncpy(value, "gb", reg_data->entries[i].key_size);
			}
			value[reg_data->entries[i].key_size] = '\0';
		} else if (key_id == reg_id_yob) {  // dummy year
			*((int *)(reg_data->entries[i].key_value)) = 2000;
		} else if (key_id == reg_id_mob) {  // dummy month
			*((int *)(reg_data->entries[i].key_value)) = 1;
		} else if (key_id == reg_id_dob) {  // dummy day
			*((int *)(reg_data->entries[i].key_value)) = 1;
		} else if (key_id == reg_id_env) {  // keep environment
			value = (char *)(reg_data->entries[i].key_value);
			sceRegMgrGetKeyStr(reg_data->entries[i].key_path, reg_data->entries[i].key_name, value, reg_data->entries[i].key_size);
			if (value[0] == '\0') {  // fallback dummy
				sceClibStrncpy(value, "np", reg_data->entries[i].key_size);
			}
			value[reg_data->entries[i].key_size] = '\0';
		}
	}

	return;
}

void get_current_reg_data(struct Registry_Data *reg_data)
{
	int i;

	for (i = 0; i < reg_data->count; i++) {
		if (reg_data->entries[i].key_value == NULL) {
			continue;
		}
		sceClibMemset(reg_data->entries[i].key_value, 0x00, reg_data->entries[i].key_size);

		switch(reg_data->entries[i].key_type) {
			case KEY_TYPE_INT:
				sceRegMgrGetKeyInt(reg_data->entries[i].key_path, reg_data->entries[i].key_name, (int *)(reg_data->entries[i].key_value));
				break;
			case KEY_TYPE_STR:
				sceRegMgrGetKeyStr(reg_data->entries[i].key_path, reg_data->entries[i].key_name, (char *)(reg_data->entries[i].key_value), reg_data->entries[i].key_size);
				((char *)(reg_data->entries[i].key_value))[reg_data->entries[i].key_size] = '\0';
				break;
			case KEY_TYPE_BIN:
				sceRegMgrGetKeyBin(reg_data->entries[i].key_path, reg_data->entries[i].key_name, reg_data->entries[i].key_value, reg_data->entries[i].key_size);
				break;
		}
	}

	return;
}

void set_reg_data(struct Registry_Data *reg_data)
{
	int i;

	for (i = 0; i < reg_data->count; i++) {
		if (reg_data->entries[i].key_value == NULL) {
			continue;
		}
		printf("\e[2mSetting registry %s...\e[22m\e[0K\n", reg_data->entries[i].key_name);

		switch(reg_data->entries[i].key_type) {
			case KEY_TYPE_INT:
				sceRegMgrSetKeyInt(reg_data->entries[i].key_path, reg_data->entries[i].key_name, *((int *)(reg_data->entries[i].key_value)));
				break;
			case KEY_TYPE_STR:
				((char *)(reg_data->entries[i].key_value))[reg_data->entries[i].key_size] = '\0';
				sceRegMgrSetKeyStr(reg_data->entries[i].key_path, reg_data->entries[i].key_name, (char *)(reg_data->entries[i].key_value), reg_data->entries[i].key_size);
				break;
			case KEY_TYPE_BIN:
				sceRegMgrSetKeyBin(reg_data->entries[i].key_path, reg_data->entries[i].key_name, reg_data->entries[i].key_value, reg_data->entries[i].key_size);
				break;
		}
	}

	return;
}

void init_file_data(struct File_Data *file_data)
{
	// Copy user template to new file entries array
	file_data->count = template_file_user_data.count;
	file_data->size = template_file_user_data.size;
	file_data->entries = (struct File_Entry *)malloc(template_file_user_data.size);
	sceClibMemcpy((void *)(file_data->entries), (void *)(template_file_user_data.entries), template_file_user_data.size);

	return;
}

void get_current_file_data(struct File_Data *file_data)
{
	int i;
	char source_path[(MAX_PATH_LENGTH)];

	for (i = 0; i < file_data->count; i++) {
		if ((file_data->entries[i].file_path == NULL) || (file_data->entries[i].file_name_path == NULL)) {
			file_data->entries[i].file_available = false;
			continue;
		}

		sceClibStrncpy(source_path, file_data->entries[i].file_path, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, file_data->entries[i].file_name_path, (MAX_PATH_LENGTH));
		file_data->entries[i].file_available = check_file_exists(source_path);
	}

	return;
}

void set_file_data(struct File_Data *file_data, char *username)
{
	int i;
	int size_source_path;
	char source_path[(MAX_PATH_LENGTH)];
	char target_path[(MAX_PATH_LENGTH)];
	char *value;

	// build source base path
	size_source_path = 0;
	if (username != NULL) {
		sceClibStrncpy(source_path, app_base_path, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, accounts_folder, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, username, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, slash_folder, (MAX_PATH_LENGTH));
		size_source_path = sceClibStrnlen(source_path, (MAX_PATH_LENGTH));
	}

	for (i = 0; i < file_data->count; i++) {
		if ((file_data->entries[i].file_path == NULL) || (file_data->entries[i].file_name_path == NULL)) {
			continue;
		}

		// build target path
		sceClibStrncpy(target_path, file_data->entries[i].file_path, (MAX_PATH_LENGTH));
		sceClibStrncat(target_path, file_data->entries[i].file_name_path, (MAX_PATH_LENGTH));

		if ((username != NULL) && (file_data->entries[i].file_available)) {
			// build source path
			source_path[size_source_path] = '\0';
			sceClibStrncat(source_path, file_data->entries[i].file_save_path, (MAX_PATH_LENGTH));
			sceClibStrncat(source_path, file_data->entries[i].file_name_path, (MAX_PATH_LENGTH));
			// always remove target
			if (check_file_exists(target_path)) {
				printf("\e[2mDeleting target %s...\e[22m\e[0K\n", target_path);
				sceIoRemove(target_path);
			}
			// copy source
			if (!check_file_exists(source_path)) {
				printf("\e[1mMissing source %s...\e[22m\e[0K\n", source_path);
			} else {
				// create target path directories
				value = target_path;
				while ((value = strchr(value, '/')) != NULL) {
					*value = '\0';
					if (!check_folder_exists(target_path)) {
						printf("\e[2mCreating folder %s/...\e[22m\e[0K\n", target_path);
						sceIoMkdir(target_path, 0006);
					}
					*value++ = '/';
				};
				// copy file
				printf("\e[2mCopying %s...\e[22m\e[0K\n", source_path);
				copy_file(source_path, target_path);
			}
		} else {
			if (!check_file_exists(target_path)) {
				printf("\e[2mSkip missing %s...\e[22m\e[0K\n", target_path);
			} else {
				printf("\e[2mDeleting %s...\e[22m\e[0K\n", target_path);
				sceIoRemove(target_path);
			}
		}
	}

	return;
}

void display_account_details_short(struct Registry_Data *reg_data, bool *no_user)
{
	int len;

	if (no_user != NULL) {
		*no_user = false;
	}
	// username
	printf("Current User Name: ");
	if ((reg_data->idx_username >= 0) && (reg_data->entries[reg_data->idx_username].key_value != NULL) && ((len = sceClibStrnlen((char *)(reg_data->entries[reg_data->idx_username].key_value), reg_data->entries[reg_data->idx_username].key_size)) > 0)) {
		printf("%s\e[0K\n", (char *)(reg_data->entries[reg_data->idx_username].key_value), len);
	} else {
		printf("<None>\e[0K\n");
		if (no_user != NULL) {
			*no_user = true;
		}
	}
	// login id
	printf("Current Login ID: ");
	if ((reg_data->idx_login_id >= 0) && (reg_data->entries[reg_data->idx_login_id].key_value != NULL) && ((len = sceClibStrnlen((char *)(reg_data->entries[reg_data->idx_login_id].key_value), reg_data->entries[reg_data->idx_login_id].key_size)) > 0)) {
		printf("%s\e[0K\n", (char *)(reg_data->entries[reg_data->idx_login_id].key_value), len);
	} else {
		printf("<None>\e[0K\n");
		if (no_user != NULL) {
			*no_user = true;
		}
	}

	return;
}

void display_account_details_full(struct Registry_Data *reg_data, struct File_Data *file_data, char *title)
{
	int i, j;
	int len;

	if (title != NULL) {
		// draw title line
		draw_title_line(title);

		// draw pixel line
		draw_pixel_line(NULL, NULL);
	}

	// registry data
	printf("Registry Data:\e[0K\n");
	for (i = 0; i < reg_data->count; i++) {
		printf("\e[2m%s/\e[22m%s: ", reg_data->entries[i].key_path, reg_data->entries[i].key_name);

		switch(reg_data->entries[i].key_type) {
			case KEY_TYPE_INT:
				printf("%i\e[0K\n", *((int *)(reg_data->entries[i].key_value)));
				break;
			case KEY_TYPE_STR:
				if ((reg_data->entries[i].key_value != NULL) && ((len = sceClibStrnlen((char *)(reg_data->entries[i].key_value), reg_data->entries[i].key_size)) > 0)) {
					printf("%s (%i)\e[0K\n", (char *)(reg_data->entries[i].key_value), len);
				} else {
					printf("<None>\e[0K\n");
				}
				break;
			case KEY_TYPE_BIN:
				for (j = 0; j < reg_data->entries[i].key_size; j++) {
					printf("%02x", ((unsigned char *)(reg_data->entries[i].key_value))[j]);
				}
				printf(" (%i)\e[0K\n", reg_data->entries[i].key_size);
				break;
		}
	}

	// file data
	printf("File Data:\e[0K\n");
	for (i = 0; i < file_data->count; i++) {
		if ((file_data->entries[i].file_path == NULL) || (file_data->entries[i].file_name_path == NULL)) {
			continue;
		}

		printf("\e[2m%s\e[22m%s: ", file_data->entries[i].file_path, file_data->entries[i].file_name_path);

		if (file_data->entries[i].file_available) {
			printf("available\e[0K\n");
		} else {
			printf("missing\e[0K\n");
		}
	}

	if (title != NULL) {
		wait_for_cancel_button();
	}

	return;
}

void save_account_details(struct Registry_Data *reg_data, struct File_Data *file_data, char *title)
{
	int i;
	int size;
	int size_base_path;
	int size_target_path;
	char base_path[(MAX_PATH_LENGTH)];
	char target_path[(MAX_PATH_LENGTH)];
	char source_path[(MAX_PATH_LENGTH)];
	char string[(STRING_BUFFER_DEFAULT_SIZE)];
	char *value;

	// draw title line
	draw_title_line(title);

	// draw pixel line
	draw_pixel_line(NULL, NULL);

	// check username and login id
	if ((reg_data->idx_username < 0) || (reg_data->entries[reg_data->idx_username].key_value == NULL) || (sceClibStrnlen((char *)(reg_data->entries[reg_data->idx_username].key_value), 1) == 0)  // check username
	    || (reg_data->idx_login_id < 0) || (reg_data->entries[reg_data->idx_login_id].key_value == NULL) || (sceClibStrnlen((char *)(reg_data->entries[reg_data->idx_login_id].key_value), 1) == 0))  // check login id
	{
		printf("\e[1mThere is no linked account.\e[22m\e[0K\n");
		sceKernelDelayThread(1500000);  // 1.5s
		return;
	}

	// build target base path
	sceClibStrncpy(base_path, app_base_path, (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, accounts_folder, (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, (char *)(reg_data->entries[reg_data->idx_username].key_value), (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, slash_folder, (MAX_PATH_LENGTH));
	size_base_path = sceClibStrnlen(base_path, (MAX_PATH_LENGTH));
	printf("Saving account details to %s...\e[0K\n", base_path);
	// create target base path directories
	value = base_path;
	while ((value = strchr(value, '/')) != NULL) {
		*value = '\0';
		if (!check_folder_exists(base_path)) {
			//printf("\e[2mCreating folder %s/...\e[22m\e[0K\n", base_path);
			sceIoMkdir(base_path, 0006);
		}
		*value++ = '/';
	};
	//
	sceClibStrncpy(target_path, base_path, (MAX_PATH_LENGTH));

	// save registry user data
	target_path[size_base_path] = '\0';
	size_target_path = size_base_path;
	for (i = 0; i < reg_data->count; i++) {
		if (reg_data->entries[i].key_id == reg_id_username) {  // do not save username, already stored in account folder name
			continue;
		}

		if ((reg_data->entries[i].key_save_path == NULL) || (reg_data->entries[i].key_name == NULL)) {
			continue;
		}

		// build target path
		target_path[size_target_path] = '\0';
		sceClibStrncat(target_path, reg_data->entries[i].key_save_path, (MAX_PATH_LENGTH));
		sceClibStrncat(target_path, reg_data->entries[i].key_name, (MAX_PATH_LENGTH));
		// create target path directories
		value = &target_path[size_base_path];
		while ((value = strchr(value, '/')) != NULL) {
			*value = '\0';
			if (!check_folder_exists(target_path)) {
				//printf("\e[2mCreating folder %s/...\e[22m\e[0K\n", target_path);
				sceIoMkdir(target_path, 0006);
			}
			*value++ = '/';
		};

		switch(reg_data->entries[i].key_type) {
			case KEY_TYPE_INT:
				sceClibStrncat(target_path, file_ext_txt, (MAX_PATH_LENGTH));
				sceClibSnprintf(string, (STRING_BUFFER_DEFAULT_SIZE), "%i", *((int *)(reg_data->entries[i].key_value)));
				value = string;
				size = sceClibStrnlen(value, (reg_data->entries[i].key_size));
				break;
			case KEY_TYPE_STR:
				sceClibStrncat(target_path, file_ext_txt, (MAX_PATH_LENGTH));
				value = (char *)(reg_data->entries[i].key_value);
				size = sceClibStrnlen(value, (reg_data->entries[i].key_size));
				break;
			case KEY_TYPE_BIN:
				sceClibStrncat(target_path, file_ext_bin, (MAX_PATH_LENGTH));
				value = (char *)(reg_data->entries[i].key_value);
				size = reg_data->entries[i].key_size;
				break;
			default:  // unknown type
				continue;  // skip entry
		}
		printf("\e[2mWriting %s...\e[22m\e[0K\n", reg_data->entries[i].key_name);
		write_file(target_path, (void *)value, size);
	}

	// save file user data
	target_path[size_base_path] = '\0';
	size_target_path = size_base_path;
	for (i = 0; i < file_data->count; i++) {
		if ((file_data->entries[i].file_save_path == NULL) || (file_data->entries[i].file_name_path == NULL) || (file_data->entries[i].file_path == NULL)) {
			continue;
		}

		if (!file_data->entries[i].file_available) {
			printf("\e[2mSkip missing %s...\e[22m\e[0K\n", file_data->entries[i].file_name_path);
			continue;
		}

		// build target path
		target_path[size_target_path] = '\0';
		sceClibStrncat(target_path, file_data->entries[i].file_save_path, (MAX_PATH_LENGTH));
		sceClibStrncat(target_path, file_data->entries[i].file_name_path, (MAX_PATH_LENGTH));
		// create target path directories
		value = &target_path[size_base_path];
		while ((value = strchr(value, '/')) != NULL) {
			*value = '\0';
			if (!check_folder_exists(target_path)) {
				//printf("\e[2mCreating folder %s/...\e[22m\e[0K\n", target_path);
				sceIoMkdir(target_path, 0006);
			}
			*value++ = '/';
		};

		// build source path
		sceClibStrncpy(source_path, file_data->entries[i].file_path, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, file_data->entries[i].file_name_path, (MAX_PATH_LENGTH));

		// copy file
		printf("\e[2mCopying %s...\e[22m\e[0K\n", file_data->entries[i].file_name_path);
		copy_file(source_path, target_path);
	}

	printf("Account %s saved!\e[0K\n", (char *)(reg_data->entries[reg_data->idx_username].key_value));

	wait_for_cancel_button();

	return;
}

void read_account_details(struct Registry_Data *reg_data, struct File_Data *file_data, struct Registry_Data *reg_init_data, struct File_Data *file_init_data)
{
	int i;
	int size;
	int size_base_path;
	int size_source_path;
	char base_path[(MAX_PATH_LENGTH)];
	char source_path[(MAX_PATH_LENGTH)];
	char string[(STRING_BUFFER_DEFAULT_SIZE)];
	char *value;
	bool check;

	// build source base path
	sceClibStrncpy(base_path, app_base_path, (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, accounts_folder, (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, (char *)(reg_data->entries[reg_data->idx_username].key_value), (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, slash_folder, (MAX_PATH_LENGTH));
	size_base_path = sceClibStrnlen(base_path, (MAX_PATH_LENGTH));
	printf("Reading account details from %s...\e[0K\n", base_path);
	//
	sceClibStrncpy(source_path, base_path, (MAX_PATH_LENGTH));

	// load registry user data
	source_path[size_base_path] = '\0';
	size_source_path = size_base_path;
	for (i = 0; i < reg_data->count; i++) {
		if (reg_data->entries[i].key_id == reg_id_username) {  // do not read username, already stored in reg data from account folder name
			continue;
		}

		check = false;
		if ((reg_data->entries[i].key_save_path != NULL) && (reg_data->entries[i].key_name != NULL)) {
			check = true;
			// build source path
			source_path[size_source_path] = '\0';
			sceClibStrncat(source_path, reg_data->entries[i].key_save_path, (MAX_PATH_LENGTH));
			sceClibStrncat(source_path, reg_data->entries[i].key_name, (MAX_PATH_LENGTH));
			switch(reg_data->entries[i].key_type) {
				case KEY_TYPE_INT:
					sceClibStrncat(source_path, file_ext_txt, (MAX_PATH_LENGTH));
					break;
				case KEY_TYPE_STR:
					sceClibStrncat(source_path, file_ext_txt, (MAX_PATH_LENGTH));
					break;
				case KEY_TYPE_BIN:
					sceClibStrncat(source_path, file_ext_bin, (MAX_PATH_LENGTH));
					break;
				default:  // unknown type
					// TODO: error message unknown type
					continue;  // skip entry
			}
		}

		// check and read source path
		if ((!check) || (!check_file_exists(source_path))) {
			if (!check) {
				printf("\e[2mUse initial %s...\e[22m\e[0K\n", reg_data->entries[i].key_name);
			} else {
				printf("\e[2mUse initial for missing %s...\e[22m\e[0K\n", source_path);
			}
			switch(reg_data->entries[i].key_type) {
				case KEY_TYPE_INT:
					*((int *)(reg_data->entries[i].key_value)) = *((int *)(reg_init_data->entries[i].key_value));
					break;
				case KEY_TYPE_STR:
					sceClibStrncpy((char *)(reg_data->entries[i].key_value), (char *)(reg_init_data->entries[i].key_value), (reg_data->entries[i].key_size) - 1);
					((char *)(reg_data->entries[i].key_value))[reg_data->entries[i].key_size] = '\0';
					break;
				case KEY_TYPE_BIN:
					sceClibMemcpy(reg_data->entries[i].key_value, reg_init_data->entries[i].key_value, reg_data->entries[i].key_size);
					break;
				default:  // unknown type
					// TODO: error message unknown type
					continue;  // skip entry
			}
		} else {
			size = allocate_read_file(source_path, ((void **)(&value)));
			printf("\e[2mReading %s... (%i)\e[22m\e[0K\n", source_path, size);
			switch(reg_data->entries[i].key_type) {
				case KEY_TYPE_INT:
					size = min(size, (STRING_BUFFER_DEFAULT_SIZE) - 1);
					sceClibMemcpy((void *)string, (void *)value, size);
					string[size] = '\0';
					*((int *)(reg_data->entries[i].key_value)) = atoi(string);
					break;
				case KEY_TYPE_STR:
					size = min(size, (reg_data->entries[i].key_size) - 1);
					sceClibStrncpy((char *)(reg_data->entries[i].key_value), value, size);
					((char *)(reg_data->entries[i].key_value))[size] = '\0';
					break;
				case KEY_TYPE_BIN:
					size = min(size, reg_data->entries[i].key_size);
					sceClibMemcpy(reg_data->entries[i].key_value, (void *)value, size);
					break;
				default:  // unknown type
					// TODO: error message unknown type
					continue;  // skip entry
			}
			free(value);
		}
	}

	// load file user data
	source_path[size_base_path] = '\0';
	size_source_path = size_base_path;
	for (i = 0; i < file_data->count; i++) {
		if ((file_data->entries[i].file_save_path == NULL) || (file_data->entries[i].file_path == NULL) || (file_data->entries[i].file_name_path == NULL)) {
			file_data->entries[i].file_available = false;
			continue;
		}

		// build source path
		source_path[size_source_path] = '\0';
		sceClibStrncat(source_path, file_data->entries[i].file_save_path, (MAX_PATH_LENGTH));
		sceClibStrncat(source_path, file_data->entries[i].file_name_path, (MAX_PATH_LENGTH));
		file_data->entries[i].file_available = check_file_exists(source_path);
	}
}

bool switch_account(struct Registry_Data *reg_data, struct Registry_Data *reg_init_data, struct File_Data *file_init_data, char *title)
{
	bool result;
	bool menu_redraw;
	bool menu_redraw_screen;
	bool menu_run;
	int menu_items;
	int menu_item;
	int x, y;
	int x2, y2;
	int x3, y3;
	int button_pressed;
	int i;
	int size_base_path;
	char base_path[(MAX_PATH_LENGTH)];
	SceUID dfd;
	SceIoDirent entry;
	struct Dir_Entry *dirs;
	int dir_count;
	int dir_count2;
	int dirs_per_screen;
	int size;
	int count;

	result = false;

	// build source base path
	sceClibStrncpy(base_path, app_base_path, (MAX_PATH_LENGTH));
	sceClibStrncat(base_path, accounts_folder, (MAX_PATH_LENGTH));
	size_base_path = sceClibStrnlen(base_path, (MAX_PATH_LENGTH));

	// read directories in base path
	dirs = NULL;
	dir_count = 0;
	dir_count2 = 0;
	base_path[size_base_path - 1] = '\0';
	count = 2;
	do {
		count--;

		if ((dir_count > 0) && (dirs == NULL)) {
			dirs = (struct Dir_Entry *)malloc(dir_count * sizeof(struct Dir_Entry));
		}

		dfd = sceIoDopen(base_path);
		if (dfd >= 0) {
			sceClibMemset(&entry, 0, sizeof(SceIoDirent));
			while (sceIoDread(dfd, &entry) > 0) {
				if (!(SCE_S_ISDIR(entry.d_stat.st_mode))) {
					continue;
				}

				if (dirs == NULL) {
					dir_count++;
				} else {
					dirs[dir_count2].size = sceClibStrnlen(entry.d_name, sizeof(entry.d_name));
					dirs[dir_count2].name = (char *)malloc(dirs[dir_count2].size + 1);
					sceClibStrncpy(dirs[dir_count2].name , entry.d_name, dirs[dir_count2].size);
					dirs[dir_count2].name[dirs[dir_count2].size] = '\0';
					dir_count2++;
				}
			}
			sceIoDclose(dfd);
		}
	} while ((count > 0) && (dir_count > 0));
	base_path[size_base_path - 1] = '/';

	// run switch menu
	menu_redraw_screen = true;
	menu_redraw = true;
	menu_run = true;
	menu_items = 0;
	menu_item = 0;
	dir_count2 = 0;
	count = dir_count2;
	do {
		// redraw screen
		if (menu_redraw_screen) {
			// draw title line
			draw_title_line(title);

			// draw pixel line
			draw_pixel_line(NULL, NULL);
			psvDebugScreenGetCoordsXY(NULL, &y3);  // start of data
			x3 = 0;

			// draw current account data
			display_account_details_short(reg_data, NULL);

			// draw pixel line
			draw_pixel_line(NULL, NULL);

			// draw info
			printf("Switch to another account by changing account data\e[0K\n");
			printf("in registry and replacing account files.\e[0K\n");
			printf("\e[2K\nThe following %i accounts are available: (L/R to page)\e[0K\n", dir_count);

			// draw first part of menu
			psvDebugScreenGetCoordsXY(NULL, &y);  // start of menu
			x = 0;
			printf(" Cancel.\e[0K\n");
			psvDebugScreenGetCoordsXY(NULL, &y2);  // start of account list
			x2 = 0;

			dirs_per_screen = ((SCREEN_HEIGHT) - y2 + 1) / psv_font_current->size_h;

			menu_redraw = true;
			menu_redraw_screen = false;
		}

		// redraw account list
		if (menu_redraw) {
			psvDebugScreenSetCoordsXY(&x2, &y2);
			printf("\e[0J");

			count = dir_count2;
			menu_items = 0;
			for (i = 0; (i < dirs_per_screen) && (count < dir_count); i++, count++) {
				menu_items++;
				size = (dirs[count].size > (reg_init_data->entries[reg_init_data->idx_username].key_size - 1));
				if (size) {
					printf("\e[2m");
				}
				printf(" %.*s", reg_init_data->entries[reg_init_data->idx_username].key_size, dirs[count].name);
				if (size) {
					printf("... (name too long)\e[22m");
				}
				printf("\e[0K\n");
			}

			menu_redraw = false;
		}

		// draw menu marker
		psvDebugScreenSetCoordsXY(&x, &y);
		//
		if (menu_item < 0) {
			menu_item = 0;
		}
		if (menu_item > menu_items) {
			menu_item = menu_items;
		}
		//
		for (i = 0; i <= menu_items; i++) {
			if (menu_item == i) {
				printf(">\n");
			} else {
				printf(" \n");
			}
		}

		// process key strokes
		button_pressed = get_key();
		if (button_pressed == SCE_CTRL_DOWN) {
			menu_item++;
		} else if (button_pressed == SCE_CTRL_UP) {
			menu_item--;
		} else if (button_pressed == SCE_CTRL_RTRIGGER) {
			if (count < dir_count) {
				dir_count2 += dirs_per_screen;
				if (dir_count2 >= dir_count) {
					dir_count2 = dir_count - 1;
				}
				menu_redraw = true;
			}
		} else if (button_pressed == SCE_CTRL_LTRIGGER) {
			if (dir_count2 > 0) {
				dir_count2 -= dirs_per_screen;
				if (dir_count2 < 0) {
					dir_count2 = 0;
				}
				menu_redraw = true;
			}
		} else if (button_pressed == button_enter) {
			if (menu_item == 0) {  // cancel
				menu_run = false;
			} else if (menu_item > 0) {  // switch account
				i = dir_count2 + menu_item - 1;
				size = (dirs[i].size > (reg_init_data->entries[reg_init_data->idx_username].key_size - 1));
				if (!size) {
					struct Registry_Data reg_new_data;
					struct File_Data file_new_data;

					// clear data part of screen
					psvDebugScreenSetCoordsXY(&x3, &y3);
					printf("\e[0J");
					// initialize data for user to be switched to
					init_reg_data(&reg_new_data);
					init_file_data(&file_new_data);
					// copy username
					sceClibStrncpy((char *)(reg_new_data.entries[reg_new_data.idx_username].key_value), dirs[i].name, (reg_new_data.entries[reg_new_data.idx_username].key_size - 1));
					// read user data
					read_account_details(&reg_new_data, &file_new_data, reg_init_data, file_init_data);
					// check for sufficient user data (login_id)
					if ((reg_new_data.idx_login_id < 0) || (reg_new_data.entries[reg_new_data.idx_login_id].key_value == NULL) || (sceClibStrnlen((char *)(reg_new_data.entries[reg_new_data.idx_login_id].key_value), 1) == 0))  // check login id
					{
						printf("\e[1mAccount %s data is insufficient (at least login id is needed).\e[22m\e[0K\n", (char *)(reg_new_data.entries[reg_new_data.idx_username].key_value));
						wait_for_cancel_button();
						menu_redraw = true;
						menu_redraw_screen = true;
					} else {
						// set registry user data
						set_reg_data(&reg_new_data);
						// copy/remove file user data
						set_file_data(&file_new_data, reg_new_data.entries[reg_new_data.idx_username].key_value);
						// delete execution history data
						delete_execution_history(&execution_history_data, NULL);
						//
						printf("Account %s restored!\e[0K\n", (char *)(reg_new_data.entries[reg_new_data.idx_username].key_value));
						wait_for_cancel_button();
						menu_run = false;
						result = true;
					}
					free_reg_data(&reg_new_data);
				}
			}
		}
	} while (menu_run);

	// free memory
	for (i = 0; i < dir_count; i++) {
		free(dirs[i].name);
	}
	free(dirs);

	return result;
}

bool remove_account(struct Registry_Data *reg_data, struct Registry_Data *reg_init_data, struct File_Data *file_init_data, char *title)
{
	bool result;
	bool menu_run;
	int menu_items;
	int menu_item;
	int x, y;
	int button_pressed;
	int i;

	result = false;

	// draw title line
	draw_title_line(title);

	// draw pixel line
	draw_pixel_line(NULL, NULL);

	// draw current account data
	display_account_details_short(reg_data, NULL);

	// draw pixel line
	draw_pixel_line(NULL, NULL);

	// draw info
	printf("Remove current account by setting initial account data\e[0K\n");
	printf("in registry and deleting account files.\e[0K\n");
	printf("\e[2K\nContinue?\e[0K\n");

	// draw menu
	psvDebugScreenGetCoordsXY(NULL, &y);
	x = 0;
	menu_run = true;
	menu_items = 0;
	menu_item = 0;
	printf(" Cancel.\e[0K\n"); menu_items++;
	printf(" Remove account.\e[0K\n");

	do {
		// draw menu marker
		psvDebugScreenSetCoordsXY(&x, &y);
		//
		if (menu_item < 0) {
			menu_item = 0;
		}
		if (menu_item > menu_items) {
			menu_item = menu_items;
		}
		//
		for (i = 0; i <= menu_items; i++) {
			if (menu_item == i) {
				printf(">\n");
			} else {
				printf(" \n");
			}
		}

		// process key strokes
		button_pressed = get_key();
		if (button_pressed == SCE_CTRL_DOWN) {
			menu_item++;
		} else if (button_pressed == SCE_CTRL_UP) {
			menu_item--;
		} else if (button_pressed == button_enter) {
			if (menu_item == 0) {  // cancel
				menu_run = false;
			} else if (menu_item == 1) {  // remove account
				set_reg_data(reg_init_data);
				set_file_data(file_init_data, NULL);
				delete_execution_history(&execution_history_data, NULL);
				//
				printf("Account %s removed!\e[0K\n", (char *)(reg_data->entries[reg_data->
idx_username].key_value));
				wait_for_cancel_button();
				menu_run = false;
				result = true;
			}
		}
	} while (menu_run);

	return result;
}

void main_account(void)
{
	int i;

	// determine special indexes of registry data
	template_reg_user_data.idx_username = -1;
	template_reg_user_data.idx_login_id = -1;
	for (i = 0; i < template_reg_user_data.count; i++) {
		// username
		if ((template_reg_user_data.idx_username < 0) && (template_reg_user_data.entries[i].key_id == reg_id_username)) {
			template_reg_user_data.idx_username = i;
		}
		// login id
		if ((template_reg_user_data.idx_login_id < 0) && (template_reg_user_data.entries[i].key_id == reg_id_login_id)) {
			template_reg_user_data.idx_login_id = i;
		}
		// all found?
		if ((template_reg_user_data.idx_username >= 0) && (template_reg_user_data.idx_login_id >= 0)) {
			break;
		}
	}
}
