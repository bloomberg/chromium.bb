// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>

#include "base/file_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui_test_utils.h"

static const FilePath::CharType* kTestDir = FILE_PATH_LITERAL("encoding_tests");

class BrowserEncodingTest : public UITest {
 protected:
  BrowserEncodingTest() : UITest() {}

  // Make sure the content of the page are as expected
  // after override or auto-detect
  void CheckFile(const FilePath& generated_file,
                 const FilePath& expected_result_file,
                 bool check_equal) {
    FilePath expected_result_filepath = ui_test_utils::GetTestFilePath(
        FilePath(kTestDir), expected_result_file);

    ASSERT_TRUE(file_util::PathExists(expected_result_filepath));
    WaitForGeneratedFileAndCheck(generated_file,
                                 expected_result_filepath,
                                 true,  // We do care whether they are equal.
                                 check_equal,
                                 true);  // Delete the generated file when done.
  }

  virtual void SetUp() {
    UITest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    save_dir_ = temp_dir_.path();
    temp_sub_resource_dir_ = save_dir_.AppendASCII("sub_resource_files");
  }

  ScopedTempDir temp_dir_;
  FilePath save_dir_;
  FilePath temp_sub_resource_dir_;
};

// TODO(jnd): 1. Some encodings are missing here. It'll be added later. See
// http://crbug.com/13306.
// 2. Add more files with multiple encoding name variants for each canonical
// encoding name). Webkit layout tests cover some, but testing in the UI test is
// also necessary.
TEST_F(BrowserEncodingTest, TestEncodingAliasMapping) {
  struct EncodingTestData {
    const char* file_name;
    const char* encoding_name;
  };

  const EncodingTestData kEncodingTestDatas[] = {
    { "Big5.html", "Big5" },
    { "EUC-JP.html", "EUC-JP" },
    { "gb18030.html", "gb18030" },
    { "iso-8859-1.html", "ISO-8859-1" },
    { "ISO-8859-2.html", "ISO-8859-2" },
    { "ISO-8859-4.html", "ISO-8859-4" },
    { "ISO-8859-5.html", "ISO-8859-5" },
    { "ISO-8859-6.html", "ISO-8859-6" },
    { "ISO-8859-7.html", "ISO-8859-7" },
    { "ISO-8859-8.html", "ISO-8859-8" },
    { "ISO-8859-13.html", "ISO-8859-13" },
    { "ISO-8859-15.html", "ISO-8859-15" },
    { "KOI8-R.html", "KOI8-R" },
    { "KOI8-U.html", "KOI8-U" },
    { "macintosh.html", "macintosh" },
    { "Shift-JIS.html", "Shift_JIS" },
    { "US-ASCII.html", "ISO-8859-1" },  // http://crbug.com/15801
    { "UTF-8.html", "UTF-8" },
    { "UTF-16LE.html", "UTF-16LE" },
    { "windows-874.html", "windows-874" },
    { "windows-949.html", "windows-949" },
    { "windows-1250.html", "windows-1250" },
    { "windows-1251.html", "windows-1251" },
    { "windows-1252.html", "windows-1252" },
    { "windows-1253.html", "windows-1253" },
    { "windows-1254.html", "windows-1254" },
    { "windows-1255.html", "windows-1255" },
    { "windows-1256.html", "windows-1256" },
    { "windows-1257.html", "windows-1257" },
    { "windows-1258.html", "windows-1258" }
  };
  const char* const kAliasTestDir = "alias_mapping";

  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());

  FilePath test_dir_path = FilePath(kTestDir).AppendASCII(kAliasTestDir);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kEncodingTestDatas); ++i) {
    FilePath test_file_path(test_dir_path);
    test_file_path = test_file_path.AppendASCII(
        kEncodingTestDatas[i].file_name);

    NavigateToURL(URLRequestMockHTTPJob::GetMockUrl(test_file_path));

    std::string encoding;
    EXPECT_TRUE(tab_proxy->GetPageCurrentEncoding(&encoding));
    EXPECT_EQ(encoding, kEncodingTestDatas[i].encoding_name);
  }
}

// Marked as flaky: see  http://crbug.com/44668
TEST_F(BrowserEncodingTest, FLAKY_TestOverrideEncoding) {
  const char* const kTestFileName = "gb18030_with_iso88591_meta.html";
  const char* const kExpectedFileName =
      "expected_gb18030_saved_from_iso88591_meta.html";
  const char* const kOverrideTestDir = "user_override";

  FilePath test_dir_path = FilePath(kTestDir).AppendASCII(kOverrideTestDir);
  test_dir_path = test_dir_path.AppendASCII(kTestFileName);
  GURL url = URLRequestMockHTTPJob::GetMockUrl(test_dir_path);
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->NavigateToURL(url));
  WaitUntilTabCount(1);

  // Get the encoding declared in the page.
  std::string encoding;
  EXPECT_TRUE(tab_proxy->GetPageCurrentEncoding(&encoding));
  EXPECT_EQ(encoding, "ISO-8859-1");

  // Override the encoding to "gb18030".
  int64 last_nav_time = 0;
  EXPECT_TRUE(tab_proxy->GetLastNavigationTime(&last_nav_time));
  EXPECT_TRUE(tab_proxy->OverrideEncoding("gb18030"));
  EXPECT_TRUE(tab_proxy->WaitForNavigation(last_nav_time));

  // Re-get the encoding of page. It should be gb18030.
  EXPECT_TRUE(tab_proxy->GetPageCurrentEncoding(&encoding));
  EXPECT_EQ(encoding, "gb18030");

  // Dump the page, the content of dump page should be identical to the
  // expected result file.
  FilePath full_file_name = save_dir_.AppendASCII(kTestFileName);
  // We save the page as way of complete HTML file, which requires a directory
  // name to save sub resources in it. Although this test file does not have
  // sub resources, but the directory name is still required.
  EXPECT_TRUE(tab_proxy->SavePage(full_file_name, temp_sub_resource_dir_,
                                  SavePackage::SAVE_AS_COMPLETE_HTML));
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  EXPECT_TRUE(WaitForDownloadShelfVisible(browser.get()));
  FilePath expected_file_name = FilePath().AppendASCII(kOverrideTestDir);
  expected_file_name = expected_file_name.AppendASCII(kExpectedFileName);
  CheckFile(full_file_name, expected_file_name, true);
}

// The following encodings are excluded from the auto-detection test because
// it's a known issue that the current encoding detector does not detect them:
// ISO-8859-4
// ISO-8859-13
// KOI8-U
// macintosh
// windows-874
// windows-1252
// windows-1253
// windows-1257
// windows-1258

// For Hebrew, the expected encoding value is ISO-8859-8-I. See
// http://crbug.com/2927 for more details.
// FLAKY / Disabled on CrOS: see http://crbug.com/44666
#if defined(OS_CHROMEOS)
#define MAYBE_TestEncodingAutoDetect DISABLED_TestEncodingAutoDetect
#else
#define MAYBE_TestEncodingAutoDetect FLAKY_TestEncodingAutoDetect
#endif

TEST_F(BrowserEncodingTest, MAYBE_TestEncodingAutoDetect) {
  struct EncodingAutoDetectTestData {
    const char* test_file_name;   // File name of test data.
    const char* expected_result;  // File name of expected results.
    const char* expected_encoding;   // expected encoding.
  };
  const EncodingAutoDetectTestData kTestDatas[] = {
      { "Big5_with_no_encoding_specified.html",
        "expected_Big5_saved_from_no_encoding_specified.html",
        "Big5" },
      { "gb18030_with_no_encoding_specified.html",
        "expected_gb18030_saved_from_no_encoding_specified.html",
        "gb18030" },
      { "iso-8859-1_with_no_encoding_specified.html",
        "expected_iso-8859-1_saved_from_no_encoding_specified.html",
        "ISO-8859-1" },
      { "ISO-8859-5_with_no_encoding_specified.html",
        "expected_ISO-8859-5_saved_from_no_encoding_specified.html",
        "ISO-8859-5" },
      { "ISO-8859-6_with_no_encoding_specified.html",
        "expected_ISO-8859-6_saved_from_no_encoding_specified.html",
        "ISO-8859-6" },
      { "ISO-8859-7_with_no_encoding_specified.html",
        "expected_ISO-8859-7_saved_from_no_encoding_specified.html",
        "ISO-8859-7" },
      { "ISO-8859-8_with_no_encoding_specified.html",
        "expected_ISO-8859-8_saved_from_no_encoding_specified.html",
        "ISO-8859-8-I" },
      { "KOI8-R_with_no_encoding_specified.html",
        "expected_KOI8-R_saved_from_no_encoding_specified.html",
        "KOI8-R" },
      { "Shift-JIS_with_no_encoding_specified.html",
        "expected_Shift-JIS_saved_from_no_encoding_specified.html",
        "Shift_JIS" },
      { "UTF-8_with_no_encoding_specified.html",
        "expected_UTF-8_saved_from_no_encoding_specified.html",
        "UTF-8" },
      { "windows-949_with_no_encoding_specified.html",
        "expected_windows-949_saved_from_no_encoding_specified.html",
        "windows-949" },
      { "windows-1251_with_no_encoding_specified.html",
        "expected_windows-1251_saved_from_no_encoding_specified.html",
        "windows-1251" },
      { "windows-1254_with_no_encoding_specified.html",
        "expected_windows-1254_saved_from_no_encoding_specified.html",
        "windows-1254" },
      { "windows-1255_with_no_encoding_specified.html",
        "expected_windows-1255_saved_from_no_encoding_specified.html",
        "windows-1255" },
      { "windows-1256_with_no_encoding_specified.html",
        "expected_windows-1256_saved_from_no_encoding_specified.html",
        "windows-1256" }
    };
  const char* const kAutoDetectDir = "auto_detect";
  // Directory of the files of expected results.
  const char* const kExpectedResultDir = "expected_results";

  // Full path of saved file. full_file_name = save_dir_ + file_name[i];
  FilePath full_saved_file_name;

  FilePath test_dir_path = FilePath(kTestDir).AppendASCII(kAutoDetectDir);

  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  // Set the default charset to one of encodings not supported by the current
  // auto-detector (Please refer to the above comments) to make sure we
  // incorrectly decode the page. Now we use ISO-8859-4.
  ASSERT_TRUE(browser->SetStringPreference(prefs::kDefaultCharset,
                                           "ISO-8859-4"));
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestDatas);i++) {
    FilePath test_file_path(test_dir_path);
    test_file_path = test_file_path.AppendASCII(kTestDatas[i].test_file_name);
    GURL url =
        URLRequestMockHTTPJob::GetMockUrl(test_file_path);
    ASSERT_TRUE(tab->NavigateToURL(url));

    // Disable auto detect if it is on.
    EXPECT_TRUE(
        browser->SetBooleanPreference(prefs::kWebKitUsesUniversalDetector,
                                      false));
    EXPECT_TRUE(tab->Reload());

    // Get the encoding used for the page, it must be the default charset we
    // just set.
    std::string encoding;
    EXPECT_TRUE(tab->GetPageCurrentEncoding(&encoding));
    EXPECT_EQ(encoding, "ISO-8859-4");

    // Enable the encoding auto detection.
    EXPECT_TRUE(browser->SetBooleanPreference(
        prefs::kWebKitUsesUniversalDetector, true));
    EXPECT_TRUE(tab->Reload());

    // Re-get the encoding of page. It should return the real encoding now.
    bool encoding_auto_detect = false;
    EXPECT_TRUE(
        browser->GetBooleanPreference(prefs::kWebKitUsesUniversalDetector,
                                      &encoding_auto_detect));
    EXPECT_TRUE(encoding_auto_detect);
    EXPECT_TRUE(tab->GetPageCurrentEncoding(&encoding));
    EXPECT_EQ(encoding, kTestDatas[i].expected_encoding);

    // Dump the page, the content of dump page should be equal with our expect
    // result file.
    full_saved_file_name = save_dir_.AppendASCII(kTestDatas[i].test_file_name);
    // Full path of expect result file.
    FilePath expected_result_file_name = FilePath().AppendASCII(kAutoDetectDir);
    expected_result_file_name = expected_result_file_name.AppendASCII(
        kExpectedResultDir);
    expected_result_file_name = expected_result_file_name.AppendASCII(
        kTestDatas[i].expected_result);
    EXPECT_TRUE(tab->SavePage(full_saved_file_name, temp_sub_resource_dir_,
                              SavePackage::SAVE_AS_COMPLETE_HTML));
    EXPECT_TRUE(WaitForDownloadShelfVisible(browser.get()));
    CheckFile(full_saved_file_name, expected_result_file_name, true);
  }
}
