// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/utility/importer/firefox_importer_unittest_utils.h"
#include "chrome/utility/importer/nss_decryptor.h"
#include "sql/connection.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(jschuh): Disabled on Win64 build. http://crbug.com/179688
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
#define MAYBE_NSS(x) DISABLED_##x
#else
#define MAYBE_NSS(x) x
#endif

// The following test requires the use of the NSSDecryptor, on OSX this needs
// to run in a separate process, so we use a proxy object so we can share the
// same test between platforms.
TEST(FirefoxImporterTest, MAYBE_NSS(Firefox3NSS3Decryptor)) {
  base::FilePath nss_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &nss_path));
#if defined(OS_MACOSX)
  nss_path = nss_path.AppendASCII("firefox3_nss_mac");
#else
  nss_path = nss_path.AppendASCII("firefox3_nss");
#endif  // !OS_MACOSX
  base::FilePath db_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &db_path));
  db_path = db_path.AppendASCII("firefox3_profile");

  FFUnitTestDecryptorProxy decryptor_proxy;
  ASSERT_TRUE(decryptor_proxy.Setup(nss_path));

  ASSERT_TRUE(decryptor_proxy.DecryptorInit(nss_path, db_path));
  EXPECT_EQ(base::ASCIIToUTF16("hello"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECKa"
                              "jtRg4qFSHBAhv9luFkXgDJA=="));
  // Test UTF-16 encoding.
  EXPECT_EQ(base::WideToUTF16(L"\x4E2D"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECLW"
                              "qqiccfQHWBAie74hxnULxlw=="));
}

// The following test verifies proper detection of authentication scheme in
// firefox's signons db. We insert two entries into moz_logins table. The first
// has httpRealm column filled with non-empty string, therefore resulting
// PasswordForm should have SCHEME_BASIC in scheme. The second entry has NULL
// httpRealm, so it should produce a SCHEME_HTML PasswordForm.
TEST(FirefoxImporterTest, MAYBE_NSS(FirefoxNSSDecryptorDeduceAuthScheme)) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath signons_path = temp_dir.path().AppendASCII("signons.sqlite");
  sql::Connection db_conn;

  ASSERT_TRUE(db_conn.Open(signons_path));

  ASSERT_TRUE(db_conn.Execute(
      "CREATE TABLE moz_logins (id INTEGER PRIMARY KEY, hostname TEXT NOT "
      "NULL, httpRealm TEXT, formSubmitURL TEXT, usernameField TEXT NOT NULL,"
      "passwordField TEXT NOT NULL, encryptedUsername TEXT NOT NULL,"
      "encryptedPassword TEXT NOT NULL, guid TEXT, encType INTEGER,"
      "timeCreated INTEGER, timeLastUsed INTEGER, timePasswordChanged "
      "INTEGER, timesUsed INTEGER)"));

  ASSERT_TRUE(db_conn.Execute(
      "CREATE TABLE moz_disabledHosts (id INTEGER PRIMARY KEY, hostname TEXT "
      "UNIQUE ON CONFLICT REPLACE)"));

  ASSERT_TRUE(db_conn.Execute(
      "INSERT INTO 'moz_logins' VALUES(1,'http://server.com:1234',"
      "'http_realm',NULL,'','','','','{bfa37106-a4dc-0a47-abb4-dafd507bf2db}',"
      "1,1401883410959,1401883410959,1401883410959,1)"));

  ASSERT_TRUE(db_conn.Execute(
      "INSERT INTO 'moz_logins' VALUES(2,'http://server.com:1234',NULL,"
      "'http://test2.server.com:1234/action','username','password','','',"
      "'{71ad64fa-b5d4-cf4d-b390-2e4d56fe2aff}',1,1401883939239,"
      "1401883939239, 1401883939239,1)"));

  db_conn.Close();

  base::FilePath nss_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &nss_path));
#if defined(OS_MACOSX)
  nss_path = nss_path.AppendASCII("firefox3_nss_mac");
#else
  nss_path = nss_path.AppendASCII("firefox3_nss");
#endif  // !OS_MACOSX
  base::FilePath db_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &db_path));
  db_path = db_path.AppendASCII("firefox3_profile");

  FFUnitTestDecryptorProxy decryptor_proxy;
  ASSERT_TRUE(decryptor_proxy.Setup(nss_path));

  ASSERT_TRUE(decryptor_proxy.DecryptorInit(nss_path, db_path));
  std::vector<autofill::PasswordForm> forms =
      decryptor_proxy.ParseSignons(signons_path);

  ASSERT_EQ(2u, forms.size());
  EXPECT_EQ(autofill::PasswordForm::SCHEME_BASIC, forms[0].scheme);
  EXPECT_EQ(autofill::PasswordForm::SCHEME_HTML, forms[1].scheme);
}
