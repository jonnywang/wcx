/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: ixqbar@gmail.com                                             |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <time.h>
#include "php.h"
#include "php_ini.h"
#include "ext/standard/php_rand.h"
#include "ext/standard/info.h"
#include "ext/standard/crc32.h"

#include "php_wcx.h"
#include "wcx_task.h"
#include "wcx_data.h"
#include "wcx_ini.h"
#include "aes.c"
#include "zlib.c"

/* True global resources - no need for thread safety here */
static int le_wcx;

ZEND_DECLARE_MODULE_GLOBALS(wcx);

ZEND_BEGIN_ARG_INFO_EX(arg_info_wcx_encrypt, 0, 0, 2)
	ZEND_ARG_INFO(0, encrypt_str)
	ZEND_ARG_INFO(0, encrypt_key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_wcx_decrypt, 0, 0, 2)
	ZEND_ARG_INFO(0, decrypt_str)
	ZEND_ARG_INFO(0, decrypt_key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_wcx_array_rand, 0, 0, 2)
	ZEND_ARG_INFO(0, arr)
	ZEND_ARG_INFO(0, arr_num)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_wcx_bet, 0, 0, 1)
    ZEND_ARG_INFO(0, random_rate)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_wcx_task_delete, 0, 0, 1)
    ZEND_ARG_INFO(0, task_delete_arg)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_wcx_task_post, 0, 0, 1)
    ZEND_ARG_INFO(0, task_post_arg)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_wcx_ini, 0, 0, 2)
	ZEND_ARG_INFO(0, ini_file_path)
	ZEND_ARG_INFO(0, setion)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_wcx_str_rand, 0, 0, 2)
	ZEND_ARG_INFO(0, len)
	ZEND_ARG_INFO(0, option)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arg_info_wcx_array_remove, 0, 0, 3)
	ZEND_ARG_INFO(0, arr)
	ZEND_ARG_INFO(0, val)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()

/* {{{ wcx_functions[]
 *
 * Every user visible function must have an entry in wcx_functions[].
 */
const zend_function_entry wcx_functions[] = {
	PHP_FE(wcx_version,	    NULL)
	PHP_FE(wcx_encrypt,	    arg_info_wcx_encrypt)
	PHP_FE(wcx_decrypt,	    arg_info_wcx_decrypt)
	PHP_FE(wcx_array_rand,  arg_info_wcx_array_rand)
    PHP_FE(wcx_bet,         arg_info_wcx_bet)
    PHP_FE(wcx_lock,        NULL)
    PHP_FE(wcx_unlock,      NULL)
    PHP_FE(wcx_task_info,   NULL)
    PHP_FE(wcx_task_clear,  NULL)
    PHP_FE(wcx_task_delete, arg_info_wcx_task_delete)
    PHP_FE(wcx_task_post,   arg_info_wcx_task_post)
	PHP_FE(wcx_ini,         arg_info_wcx_ini)
	PHP_FE(wcx_str_rand,    arg_info_wcx_str_rand)
	PHP_FE(wcx_array_remove,arg_info_wcx_array_remove)
	PHP_FE_END	/* Must be the last line in wcx_functions[] */
};
/* }}} */

/* {{{ wcx_module_entry
 */
zend_module_entry wcx_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"wcx",
	wcx_functions,
	PHP_MINIT(wcx),
	PHP_MSHUTDOWN(wcx),
	PHP_RINIT(wcx),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(wcx),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(wcx),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_WCX_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_WCX
ZEND_GET_MODULE(wcx)
#endif

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
    PHP_INI_ENTRY("wcx.debug",     	    "0",              PHP_INI_USER,   NULL)
    PHP_INI_ENTRY("wcx.task_enabled",  	"0", 			  PHP_INI_USER,   NULL)
    PHP_INI_ENTRY("wcx.task_queue_key", "wcx_task_queue", PHP_INI_SYSTEM, NULL)
    PHP_INI_ENTRY("wcx.task_data_key",  "wcx_task_data",  PHP_INI_SYSTEM, NULL)
PHP_INI_END()
/* }}} */

static void php_wcx_init_globals(zend_wcx_globals *wcx_globals TSRMLS_DC) {
	wcx_globals->wcx_task_running = 0;
	wcx_globals->wcx_task_ptr     = (void *)wcx_task_init();
}

static void php_wcx_shutdown_globals() {
	if (WCX_G(wcx_task_ptr)) {
		wcx_task_release(((wcx_task_ptr *)WCX_G(wcx_task_ptr)));
	}
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(wcx)
{
	//ini
	REGISTER_INI_ENTRIES();
	//init
	ZEND_INIT_MODULE_GLOBALS(wcx, php_wcx_init_globals, NULL);
	//task
	register_wcx_task_class(TSRMLS_C);
	//data
	register_wcx_data_class(TSRMLS_C);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(wcx)
{
	php_wcx_shutdown_globals();
	//ini
	UNREGISTER_INI_ENTRIES();

	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(wcx)
{
	srand(time(NULL));
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(wcx)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(wcx)
{
	//info
    php_info_print_table_start();
    php_info_print_table_header(2, "wcx support", "enabled");
    php_info_print_table_row(2, "version", PHP_WCX_VERSION);
    php_info_print_table_row(2, "author",  "xingqiba");
    php_info_print_table_row(2, "website", "http://xingqiba.sinaapp.com");
    php_info_print_table_row(2, "contact", "ixqbar@gmail.com or qq174171262");
    php_info_print_table_end();

    //
    DISPLAY_INI_ENTRIES();
}
/* }}} */


PHP_FUNCTION(wcx_version) {
	RETURN_STRING(PHP_WCX_VERSION, 1);
}

PHP_FUNCTION(wcx_encrypt) {
	char *encrypt_data = NULL, *encrypt_key=NULL;
	int encrypt_data_len, encrypt_key_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &encrypt_data, &encrypt_data_len, &encrypt_key, &encrypt_key_len) == FAILURE) {
		RETURN_NULL();
	}

	if (0 == encrypt_data_len
		|| 0 == encrypt_key_len) {
		RETURN_NULL();
	}

	int crc = 0^0xFFFFFFFF;
	int n = encrypt_data_len;

	while (n--) {
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc32tab[(crc ^ (*encrypt_data++)) & 0xFF ];
	}
	crc = crc^0xFFFFFFFF;

	//reset pointer to begin
	encrypt_data -= encrypt_data_len;

	//check to gzcompress
	int gzip = encrypt_data_len > 100 ? 1 : 0;

	char *aes_data;
	size_t aes_data_len = encrypt_data_len;

	if (gzip) {
		int level = -1;
		int encoding = 0x0f;

		if (SUCCESS != php_zlib_encode(encrypt_data, encrypt_data_len, &aes_data, &aes_data_len, encoding, level)) {
			RETURN_NULL();
		}
	} else {
		aes_data = estrndup(encrypt_data, encrypt_data_len);
	}

	//header
	char header[33];
	sprintf(header, "ok,%d,%lu,%u", gzip, aes_data_len, crc);
	size_t i;
	for (i = strlen(header); i < 32; i++) {
		header[i] = ' ';
	}
	header[32] = '\0';

	//rand iv
	char aes_iv[17];
	int j;
	for (j = 0; j < 16; j++) {
		int ch = rand() % 255;
		aes_iv[j] = (char)ch;
	}
	aes_iv[16] = '\0';

	int encrypt_times = (aes_data_len + 32) / 16 + 1;
	int paddings      = 16 - (aes_data_len + 32) % 16;
	int aes_buf_size  = (aes_data_len + 32) + paddings;

	char *result = (char *)emalloc(16 + sizeof(char) * aes_buf_size);
	memcpy(result, aes_iv, 16);

	char * aes_origin_buf = (char *)emalloc(sizeof(char) * aes_buf_size);
	memset(aes_origin_buf, 0, sizeof(char) * aes_buf_size);
	memcpy(aes_origin_buf, header, 32);
	memcpy(aes_origin_buf + 32, aes_data, aes_data_len);

	char * aes_finnal_buf = (char *)emalloc(sizeof(char) * aes_buf_size);
	memset(aes_finnal_buf, 0, sizeof(char) * aes_buf_size);

	char aes_key[17];
	memset(aes_key, ' ', sizeof(aes_key));
	if (encrypt_key_len < 16) {
		memcpy(aes_key, encrypt_key, encrypt_key_len);
	} else {
		memcpy(aes_key, encrypt_key, 16);
	}

	aes_context aes_ctx;
	aes_set_key((unsigned char *)aes_key, 16, &aes_ctx);

	if (SUCCESS != aes_cbc_encrypt((unsigned char *)aes_origin_buf, (unsigned char *)aes_finnal_buf, encrypt_times, (unsigned char *)aes_iv, &aes_ctx)) {
		efree(aes_data);
		efree(aes_origin_buf);
		efree(aes_finnal_buf);
		RETURN_NULL();
	}

	memcpy(result + 16, aes_finnal_buf, aes_buf_size);

	efree(aes_data);
	efree(aes_origin_buf);
	efree(aes_finnal_buf);

    RETVAL_STRINGL(result, 16 + aes_buf_size, 1);
    efree(result);
    return;
}


PHP_FUNCTION(wcx_decrypt) {
	char *decrypt_data = NULL, *decrypt_key=NULL;
	int decrypt_data_len, decrypt_key_len;

	array_init(return_value);
	add_index_bool(return_value, 0, 1);
	add_index_string(return_value, 1, "", 1);

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &decrypt_data, &decrypt_data_len, &decrypt_key, &decrypt_key_len) == FAILURE) {
		return;
	}

	if (decrypt_data_len < 48
		|| decrypt_data_len % 16 != 0
		|| 0 == decrypt_key_len) {
		return;
	}

	//iv
	char aes_iv[17];
	memcpy(aes_iv, decrypt_data, 16);
	aes_iv[16] = '\0';

	int aes_buf_size  = decrypt_data_len - 16;
	int decrypt_times = aes_buf_size / 16;

	char *aes_finnal_buf = (char *)emalloc(sizeof(char) * aes_buf_size);
	memset(aes_finnal_buf, 0, sizeof(char) * aes_buf_size);
	memcpy(aes_finnal_buf, decrypt_data + 16, sizeof(char) * aes_buf_size);

	char *aes_origin_buf = (char *)emalloc(sizeof(char) * aes_buf_size);
	memset(aes_origin_buf, 0, sizeof(char) * aes_buf_size);

	char aes_key[17];
	memset(aes_key, ' ', sizeof(aes_key));
	if (decrypt_key_len < 16) {
		memcpy(aes_key, decrypt_key, decrypt_key_len);
	} else {
		memcpy(aes_key, decrypt_key, 16);
	}

	aes_context aes_ctx;
	aes_set_key((unsigned char *)aes_key, 16, &aes_ctx);
	if (SUCCESS != aes_cbc_decrypt((unsigned char *)aes_finnal_buf, (unsigned char *)aes_origin_buf, decrypt_times, (unsigned char *)aes_iv, &aes_ctx)) {
		efree(aes_finnal_buf);
		efree(aes_origin_buf);
		return;
	}

	efree(aes_finnal_buf);

	char header[33];
	memcpy(header, aes_origin_buf, 32);
	header[32] = '\0';

	char* header_flags[4];
	char* flags = strtok(header, ",");
	int i;
	for (i = 0; i < 4; i++) {
		header_flags[i] = flags;
		flags = strtok(NULL, ",");
	}

	if (strcmp(header_flags[0], "ok") != 0) {
		efree(aes_origin_buf);
		return;
	}

	int is_zip = 0;
	if (strcmp(header_flags[1], "1") == 0) {
		is_zip = 1;
	}

	size_t origin_data_size = strtoul(header_flags[2], NULL, 10);

	char *origin_data;
	size_t origin_data_len = origin_data_size;

	if (is_zip) {
		int encoding = 0x0f;
		if (SUCCESS != php_zlib_decode(aes_origin_buf + 32, origin_data_size, &origin_data, &origin_data_len, encoding, 0)) {
			efree(aes_origin_buf);
			return;
		}
	} else {
		origin_data = estrndup(aes_origin_buf + 32, origin_data_len);
	}

	efree(aes_origin_buf);

	int crc = 0^0xFFFFFFFF;
	size_t n = origin_data_len;

	while (n--) {
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc32tab[(crc ^ (*origin_data++)) & 0xFF ];
	}
	crc = crc^0xFFFFFFFF;

	origin_data -= origin_data_len;

	//fixed 32bit crc error
	char crc_str[1 + strlen(header_flags[3])];
	int crc_str_len = sprintf(crc_str, "%u", crc);
	crc_str[crc_str_len] = '\0';

	if (atoi(crc_str) != atoi(header_flags[3])) {
		efree(origin_data);
		return;
	}

	add_index_bool(return_value, 0, 0);
	add_index_stringl(return_value, 1, origin_data, origin_data_len, 0);
}


PHP_FUNCTION(wcx_array_rand) {
	zval *input;
	long randval, num_req = 1;
	int num_avail, key_type, rand_num;
	char *string_key;
	uint string_key_len;
	ulong num_key;
	HashPosition pos;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|l", &input, &num_req) == FAILURE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "First argument must be array and Second argument must be number");
		RETURN_NULL();
	}

	num_avail = zend_hash_num_elements(Z_ARRVAL_P(input));

	if (ZEND_NUM_ARGS() > 1) {
		if (num_req <= 0 || num_req > num_avail) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Second argument has to be between 1 and the number of elements in the array");
			RETURN_NULL();
		}
	}

	rand_num = num_req;
	array_init_size(return_value, num_req);

	zval *temp;
	zval **entry;

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(input), &pos);
	while (num_req
		&& (key_type = zend_hash_get_current_key_ex(Z_ARRVAL_P(input), &string_key, &string_key_len, &num_key, 0, &pos)) != HASH_KEY_NON_EXISTENT
		&& zend_hash_get_current_data_ex(Z_ARRVAL_P(input), (void**)&entry, &pos) == SUCCESS) {
		randval = php_rand(TSRMLS_C);
		if ((double) (randval / (PHP_RAND_MAX + 1.0)) < (double) num_req / (double) num_avail) {
			zval_add_ref(entry);
			if (key_type == HASH_KEY_IS_STRING) {
				if (1 == rand_num) {
					add_index_stringl(return_value, 0, string_key, string_key_len - 1, 1);
					add_index_zval(return_value, 1, *entry);
				} else {
					MAKE_STD_ZVAL(temp);
					array_init_size(temp, 2);
					add_index_stringl(temp, 0, string_key, string_key_len - 1, 1);
					add_index_zval(temp, 1, *entry);

					add_next_index_zval(return_value, temp);
					temp = NULL;
				}
			} else {
				if (1 == rand_num) {
					add_index_long(return_value, 0, num_key);
					add_index_zval(return_value, 1, *entry);
				} else {
					MAKE_STD_ZVAL(temp);
					array_init_size(temp, 2);
					add_index_long(temp, 0, num_key);
					add_index_zval(temp, 1, *entry);

					add_next_index_zval(return_value, temp);
					temp = NULL;
				}
			}
			num_req--;
		}
		num_avail--;
		zend_hash_move_forward_ex(Z_ARRVAL_P(input), &pos);
	}
}


PHP_FUNCTION(wcx_bet) {
    int random_rate;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &random_rate) == FAILURE) {
    	php_error_docref(NULL TSRMLS_CC, E_WARNING, "argument must be number");
    	RETURN_FALSE;
    }

    if (random_rate <= 0) {
    	RETURN_FALSE;
    }

    if (random_rate >= 100) {
    	RETURN_TRUE;
    }

    if (rand() % 101 <= 100 - random_rate) {
    	RETVAL_FALSE;
    } else {
    	RETVAL_TRUE;
    }
}

PHP_FUNCTION(wcx_ini) {
#if ((PHP_MAJOR_VERSION != 5) || (PHP_MINOR_VERSION <= 2))
	php_error_docref(NULL TSRMLS_CC, E_WARNING, "PHP version too lower!");
	RETURN_NULL();
#endif

	char *filename = NULL;
	int filename_len = 0;
	zval *section_name = NULL;
	zval *configs = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "p|z", &filename, &filename_len, &section_name) == FAILURE) {
		RETURN_NULL();
	}

	if (filename_len == 0) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Filename cannot be empty!");
		RETURN_NULL();
	}

	struct stat sb;
	if (VCWD_STAT(filename, &sb)) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to find config file '%s'", filename);
		RETURN_NULL();
	}

	if (!S_ISREG(sb.st_mode)) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Argument is not a valid ini file '%s'", filename);
		RETURN_NULL();
	}

	zend_file_handle fh = {0};
	if ((fh.handle.fp = VCWD_FOPEN(filename, "r")) == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unable to read config file '%s'", filename);
		RETURN_NULL();
	}

	MAKE_STD_ZVAL(configs);
	ZVAL_NULL(configs);
	array_init(configs);

	fh.filename = filename;
	fh.type = ZEND_HANDLE_FP;

	WCX_G(active_ini_file_section) = NULL;
	WCX_G(parsing_flag)            = WCX_CONFIG_INI_PARSING_START;

	if (section_name && Z_STRLEN_P(section_name)) {
		WCX_G(ini_wanted_section) = section_name;
	} else {
		WCX_G(ini_wanted_section) = NULL;
	}

	if (zend_parse_ini_file(&fh, 0, 0, (zend_ini_parser_cb_t)wcx_config_ini_parser_cb, configs TSRMLS_CC) == FAILURE
			|| Z_TYPE_P(configs) != IS_ARRAY) {
		zval_ptr_dtor(&configs);
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "Parsing ini file '%s' failed", filename);
		RETURN_NULL();
	}

	if (section_name
			&& Z_STRLEN_P(section_name)) {
		zval **section;
		if (zend_symtable_find(
				Z_ARRVAL_P(configs),
				Z_STRVAL_P(section_name),
				Z_STRLEN_P(section_name) + 1,
				(void **)&section) == FAILURE) {
			zval_ptr_dtor(&configs);
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "There is no section '%s' in '%s'", Z_STRVAL_P(section_name), filename);
			RETURN_NULL();
		}

		zval tmp;
		INIT_PZVAL(&tmp);
		array_init(&tmp);
		zend_hash_copy(Z_ARRVAL(tmp), Z_ARRVAL_PP(section), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		zval_dtor(configs);

		*configs = tmp;
	}

	RETVAL_ZVAL(configs, 0, 1);
}

PHP_FUNCTION(wcx_str_rand) {
	char *ini_rand_str = "abcdefghijklmnopqrstuvwxyz0123456789";
	long str_len = 0;
	char *result;
	int i, j, init_str_len = strlen(ini_rand_str), num_args = ZEND_NUM_ARGS();
	zval *option;

	if (zend_parse_parameters(num_args TSRMLS_CC, "l|z", &str_len, &option) == FAILURE) {
		RETURN_EMPTY_STRING();
	}

	if (str_len <= 0) {
		RETURN_EMPTY_STRING();
	}

	if (num_args > 1) {
		if (Z_TYPE_P(option) != IS_BOOL && Z_TYPE_P(option) != IS_STRING) {
			RETURN_EMPTY_STRING();
		}

		if (Z_TYPE_P(option) == IS_STRING) {
			if (Z_STRLEN_P(option) == 0) {
				RETURN_EMPTY_STRING();
			}
			ini_rand_str = Z_STRVAL_P(option);
			init_str_len = Z_STRLEN_P(option);
		} else {
			init_str_len = Z_LVAL_P(option) == 1 ? init_str_len - 10 : init_str_len;
		}
	}

	result = emalloc(sizeof(char *) * (str_len + 1));
	for (i = 0; i < str_len; i++) {
		// ( rand() % (最大值-最小值+1) ) + 最小值
		j = rand() % init_str_len;
		result[i] = ini_rand_str[j];
	}
	result[str_len] = '\0';

	RETURN_STRING(result, 0);
}

PHP_FUNCTION(wcx_array_remove) {
	zval *to_remove_arr, *to_remove_val, *to_remove_copy, *tmp, res, **entry, *option;
	long to_remove_num = 0;
	int key_type, to_removed_num = 0, to_remove_arr_len, args_num = ZEND_NUM_ARGS();
	char *string_key = NULL;
	uint string_key_len = 0;
	ulong num_key;
	HashPosition pos;
	int (*is_equal_func)(zval *, zval *, zval * TSRMLS_DC) = is_equal_function;

	if (zend_parse_parameters(args_num TSRMLS_CC, "az|z", &to_remove_arr, &to_remove_val, &option) == FAILURE
			|| Z_TYPE_P(to_remove_arr) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "First argument must be array");
		RETURN_LONG(0);
	}

	to_remove_arr_len = zend_hash_num_elements(Z_ARRVAL_P(to_remove_arr));
	if (0 == to_remove_arr_len) {
		RETURN_LONG(0);
	}

	if (args_num > 2) {
		if (Z_TYPE_P(option) == IS_LONG) {
			to_remove_num = Z_LVAL_P(option);
		} else if (Z_TYPE_P(option) == IS_BOOL && 1 == Z_BVAL_P(option)) {
			is_equal_func = is_identical_function;
		}
	}

	if (to_remove_num <= 0 || to_remove_num > to_remove_arr_len) {
		to_remove_num = to_remove_arr_len;
	}

	ALLOC_INIT_ZVAL(to_remove_copy);
	array_init_size(to_remove_copy, to_remove_arr_len);
	zend_hash_copy(Z_ARRVAL_P(to_remove_copy), Z_ARRVAL_P(to_remove_arr), (copy_ctor_func_t) zval_add_ref, (void *)&tmp, sizeof(zval*));

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(to_remove_copy), &pos);
	while (to_remove_num != to_removed_num
			&& zend_hash_get_current_data_ex(Z_ARRVAL_P(to_remove_copy), (void**)&entry, &pos) == SUCCESS) {
		is_equal_func(&res, to_remove_val, *entry TSRMLS_CC);
		if (Z_LVAL(res)
				&& (key_type = zend_hash_get_current_key_ex(Z_ARRVAL_P(to_remove_copy), &string_key, &string_key_len, &num_key, 0, &pos)) != HASH_KEY_NON_EXISTENT) {
			zend_hash_del_key_or_index(Z_ARRVAL_P(to_remove_arr), string_key, string_key_len, num_key, string_key_len ? HASH_DEL_KEY : HASH_DEL_INDEX);
			to_removed_num++;
		}
		zend_hash_move_forward_ex(Z_ARRVAL_P(to_remove_copy), &pos);
	}

	zval_ptr_dtor(&to_remove_copy);

	RETURN_LONG(to_removed_num);
}
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
