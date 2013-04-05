/*
 * Copyright (c) 2011, Collabora Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above
 *       copyright notice, this list of conditions and the
 *       following disclaimer.
 *     * Redistributions in binary form must reproduce the
 *       above copyright notice, this list of conditions and
 *       the following disclaimer in the documentation and/or
 *       other materials provided with the distribution.
 *     * The names of contributors to this software may not be
 *       used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#define P11_KIT_DISABLE_DEPRECATED

#include "config.h"
#include "test.h"
#include "test-tools.h"

#include "attrs.h"
#include "compat.h"
#include "debug.h"
#include "dict.h"
#include "extract.h"
#include "message.h"
#include "mock.h"
#include "path.h"
#include "pkcs11.h"
#include "pkcs11x.h"
#include "oid.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct {
	CK_FUNCTION_LIST module;
	P11KitIter *iter;
	p11_extract_info ex;
	char *directory;
} test;

static void
setup (void *unused)
{
	CK_RV rv;

	mock_module_reset ();
	memcpy (&test.module, &mock_module, sizeof (CK_FUNCTION_LIST));
	rv = test.module.C_Initialize (NULL);
	assert_num_eq (CKR_OK, rv);

	test.iter = p11_kit_iter_new (NULL);

	p11_extract_info_init (&test.ex);

	test.directory = p11_path_expand ("$TEMP/test-extract.XXXXXX");
	if (!mkdtemp (test.directory))
		assert_not_reached ();
}

static void
teardown (void *unused)
{
	CK_RV rv;

	if (rmdir (test.directory) < 0)
		assert_not_reached ();
	free (test.directory);

	p11_extract_info_cleanup (&test.ex);
	p11_kit_iter_free (test.iter);

	rv = test.module.C_Finalize (NULL);
	assert_num_eq (CKR_OK, rv);
}

static CK_OBJECT_CLASS certificate_class = CKO_CERTIFICATE;
static CK_CERTIFICATE_TYPE x509_type = CKC_X_509;

static CK_ATTRIBUTE cacert3_authority_attrs[] = {
	{ CKA_VALUE, (void *)test_cacert3_ca_der, sizeof (test_cacert3_ca_der) },
	{ CKA_CLASS, &certificate_class, sizeof (certificate_class) },
	{ CKA_CERTIFICATE_TYPE, &x509_type, sizeof (x509_type) },
	{ CKA_LABEL, "Cacert3 Here", 12 },
	{ CKA_SUBJECT, (void *)test_cacert3_ca_subject, sizeof (test_cacert3_ca_subject) },
	{ CKA_ID, "ID1", 3 },
	{ CKA_INVALID },
};

static CK_ATTRIBUTE certificate_filter[] = {
	{ CKA_CLASS, &certificate_class, sizeof (certificate_class) },
	{ CKA_INVALID },
};

static void
test_file (void)
{
	bool ret;

	mock_module_add_object (MOCK_SLOT_ONE_ID, cacert3_authority_attrs);

	p11_kit_iter_add_callback (test.iter, p11_extract_info_load_filter, &test.ex, NULL);
	p11_kit_iter_add_filter (test.iter, certificate_filter, 1);
	p11_kit_iter_begin_with (test.iter, &test.module, 0, 0);

	if (asprintf (&test.ex.destination, "%s/%s", test.directory, "extract.pem") < 0)
		assert_not_reached ();

	ret = p11_extract_pem_bundle (test.iter, &test.ex);
	assert_num_eq (true, ret);

	test_check_file (test.directory, "extract.pem", SRCDIR "/files/cacert3.pem");

	free (test.ex.destination);
}

static void
test_file_multiple (void)
{
	bool ret;

	mock_module_add_object (MOCK_SLOT_ONE_ID, cacert3_authority_attrs);
	mock_module_add_object (MOCK_SLOT_ONE_ID, cacert3_authority_attrs);

	p11_kit_iter_add_callback (test.iter, p11_extract_info_load_filter, &test.ex, NULL);
	p11_kit_iter_add_filter (test.iter, certificate_filter, 1);
	p11_kit_iter_begin_with (test.iter, &test.module, 0, 0);

	if (asprintf (&test.ex.destination, "%s/%s", test.directory, "extract.pem") < 0)
		assert_not_reached ();

	ret = p11_extract_pem_bundle (test.iter, &test.ex);
	assert_num_eq (true, ret);

	test_check_file (test.directory, "extract.pem", SRCDIR "/files/cacert3-twice.pem");

	free (test.ex.destination);
}

static void
test_file_without (void)
{
	bool ret;

	p11_kit_iter_add_callback (test.iter, p11_extract_info_load_filter, &test.ex, NULL);
	p11_kit_iter_add_filter (test.iter, certificate_filter, 1);
	p11_kit_iter_begin_with (test.iter, &test.module, 0, 0);

	if (asprintf (&test.ex.destination, "%s/%s", test.directory, "extract.pem") < 0)
		assert_not_reached ();

	ret = p11_extract_pem_bundle (test.iter, &test.ex);
	assert_num_eq (true, ret);

	test_check_data (test.directory, "extract.pem", "", 0);

	free (test.ex.destination);
}

static void
test_directory (void)
{
	bool ret;

	mock_module_add_object (MOCK_SLOT_ONE_ID, cacert3_authority_attrs);
	mock_module_add_object (MOCK_SLOT_ONE_ID, cacert3_authority_attrs);

	p11_kit_iter_add_callback (test.iter, p11_extract_info_load_filter, &test.ex, NULL);
	p11_kit_iter_add_filter (test.iter, certificate_filter, 1);
	p11_kit_iter_begin_with (test.iter, &test.module, 0, 0);

	/* Yes, this is a race, and why you shouldn't build software as root */
	if (rmdir (test.directory) < 0)
		assert_not_reached ();
	test.ex.destination = test.directory;

	ret = p11_extract_pem_directory (test.iter, &test.ex);
	assert_num_eq (true, ret);

	test_check_directory (test.directory, ("Cacert3_Here.pem", "Cacert3_Here.1.pem", NULL));
	test_check_file (test.directory, "Cacert3_Here.pem", SRCDIR "/files/cacert3.pem");
	test_check_file (test.directory, "Cacert3_Here.1.pem", SRCDIR "/files/cacert3.pem");
}

static void
test_directory_empty (void)
{
	bool ret;

	p11_kit_iter_add_callback (test.iter, p11_extract_info_load_filter, &test.ex, NULL);
	p11_kit_iter_add_filter (test.iter, certificate_filter, 1);
	p11_kit_iter_begin_with (test.iter, &test.module, 0, 0);

	/* Yes, this is a race, and why you shouldn't build software as root */
	if (rmdir (test.directory) < 0)
		assert_not_reached ();
	test.ex.destination = test.directory;

	ret = p11_extract_pem_directory (test.iter, &test.ex);
	assert_num_eq (true, ret);

	test_check_directory (test.directory, (NULL, NULL));
}

int
main (int argc,
      char *argv[])
{
	mock_module_init ();

	p11_fixture (setup, teardown);
	p11_test (test_file, "/pem/test_file");
	p11_test (test_file_multiple, "/pem/test_file_multiple");
	p11_test (test_file_without, "/pem/test_file_without");
	p11_test (test_directory, "/pem/test_directory");
	p11_test (test_directory_empty, "/pem/test_directory_empty");
	return p11_test_run (argc, argv);
}
