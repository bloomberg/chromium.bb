// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/utility/importer/firefox_importer_unittest_utils.h"
#include "chrome/utility/importer/nss_decryptor.h"
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
  EXPECT_EQ(ASCIIToUTF16("hello"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECKa"
                              "jtRg4qFSHBAhv9luFkXgDJA=="));
  // Test UTF-16 encoding.
  EXPECT_EQ(WideToUTF16(L"\x4E2D"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECLW"
                              "qqiccfQHWBAie74hxnULxlw=="));
}
