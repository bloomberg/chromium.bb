// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/net/url_request_mock_http_job.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/url_request/url_request_unittest.h"
#include "chrome/common/pref_names.h"

const wchar_t* const kTestDir = L"encoding_tests";

class BrowserEncodingTest : public UITest {
 protected:
  BrowserEncodingTest() : UITest() {}

  // Make sure the content of the page are as expected
  // after override or auto-detect
  void CheckFile(const FilePath& generated_file,
                 const FilePath& expected_result_file,
                 bool check_equal) {
    FilePath expected_result_filepath = UITest::GetTestFilePath(
        kTestDir, expected_result_file.ToWStringHack());

    ASSERT_TRUE(file_util::PathExists(expected_result_filepath));
    WaitForGeneratedFileAndCheck(generated_file,
                                 expected_result_filepath,
                                 true,  // We do care whether they are equal.
                                 check_equal,
                                 true);  // Delete the generated file when done.
  }

  virtual void SetUp() {
    UITest::SetUp();
    EXPECT_TRUE(file_util::CreateNewTempDirectory(L"", &save_dir_));
    save_dir_ += FilePath::kSeparators[0];
  }

  std::wstring save_dir_;
};

// TODO(jnd): 1. Some encodings are missing here. It'll be added later. See
// http://crbug.com/13306.
// 2. Add more files with multiple encoding name variants for each canonical
// encoding name). Webkit layout tests cover some, but testing in the UI test is
// also necessary.
TEST_F(BrowserEncodingTest, TestEncodingAliasMapping) {
  struct EncodingTestData {
    const wchar_t* file_name;
    const wchar_t* encoding_name;
  };

  const EncodingTestData kEncodingTestDatas[] = {
    { L"Big5.html", L"Big5" },
    { L"EUC-JP.html", L"EUC-JP" },
    { L"gb18030.html", L"gb18030" },
    { L"iso-8859-1.html", L"ISO-8859-1" },
    { L"ISO-8859-2.html", L"ISO-8859-2" },
    { L"ISO-8859-4.html", L"ISO-8859-4" },
    { L"ISO-8859-5.html", L"ISO-8859-5" },
    { L"ISO-8859-6.html", L"ISO-8859-6" },
    { L"ISO-8859-7.html", L"ISO-8859-7" },
    { L"ISO-8859-8.html", L"ISO-8859-8" },
    { L"ISO-8859-13.html", L"ISO-8859-13" },
    { L"ISO-8859-15.html", L"ISO-8859-15" },
    { L"KOI8-R.html", L"KOI8-R" },
    { L"KOI8-U.html", L"KOI8-U" },
    { L"macintosh.html", L"macintosh" },
    { L"Shift-JIS.html", L"Shift_JIS" },
    { L"UTF-8.html", L"UTF-8" },
    { L"UTF-16LE.html", L"UTF-16LE" },
    { L"windows-874.html", L"windows-874" },
    { L"windows-949.html", L"windows-949" },
    { L"windows-1250.html", L"windows-1250" },
    { L"windows-1251.html", L"windows-1251" },
    { L"windows-1252.html", L"windows-1252" },
    { L"windows-1253.html", L"windows-1253" },
    { L"windows-1254.html", L"windows-1254" },
    { L"windows-1255.html", L"windows-1255" },
    { L"windows-1256.html", L"windows-1256" },
    { L"windows-1257.html", L"windows-1257" },
    { L"windows-1258.html", L"windows-1258" }
  };
  const wchar_t* const kAliasTestDir = L"alias_mapping";

  FilePath test_dir_path = FilePath::FromWStringHack(kTestDir);
  test_dir_path = test_dir_path.Append(kAliasTestDir);
  for (int i = 0; i < arraysize(kEncodingTestDatas); ++i) {
    FilePath test_file_path(test_dir_path);
    test_file_path = test_file_path.Append(kEncodingTestDatas[i].file_name);
    GURL url =
        URLRequestMockHTTPJob::GetMockUrl(test_file_path.ToWStringHack());

    scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
    ASSERT_TRUE(tab_proxy.get());
    ASSERT_TRUE(tab_proxy->NavigateToURL(url));
    WaitUntilTabCount(1);

    std::wstring encoding;
    EXPECT_TRUE(tab_proxy->GetPageCurrentEncoding(&encoding));
    EXPECT_EQ(encoding, kEncodingTestDatas[i].encoding_name);
  }
}

TEST_F(BrowserEncodingTest, TestOverrideEncoding) {
  const wchar_t* const kTestFileName =
      L"gb18030_with_iso88591_meta.html";
  const wchar_t* const kExpectedFileName =
      L"expected_gb18030_saved_from_iso88591_meta.html";
  const wchar_t* const kOverrideTestDir = L"user_override";

  FilePath test_dir_path = FilePath::FromWStringHack(kTestDir);
  test_dir_path = test_dir_path.Append(kOverrideTestDir);
  test_dir_path = test_dir_path.Append(kTestFileName);
  GURL url = URLRequestMockHTTPJob::GetMockUrl(test_dir_path.ToWStringHack());
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->NavigateToURL(url));
  WaitUntilTabCount(1);

  // Get the encoding declared in the page.
  std::wstring encoding;
  EXPECT_TRUE(tab_proxy->GetPageCurrentEncoding(&encoding));
  EXPECT_EQ(encoding, L"ISO-8859-1");

  // Override the encoding to "gb18030".
  int64 last_nav_time = 0;
  EXPECT_TRUE(tab_proxy->GetLastNavigationTime(&last_nav_time));
  EXPECT_TRUE(tab_proxy->OverrideEncoding(L"gb18030"));
  EXPECT_TRUE(tab_proxy->WaitForNavigation(last_nav_time));

  // Re-get the encoding of page. It should be gb18030.
  EXPECT_TRUE(tab_proxy->GetPageCurrentEncoding(&encoding));
  EXPECT_EQ(encoding, L"gb18030");

  // Dump the page, the content of dump page should be identical to the
  // expected result file.
  std::wstring full_file_name = save_dir_ + kTestFileName;
  // We save the page as way of complete HTML file, which requires a directory
  // name to save sub resources in it. Although this test file does not have
  // sub resources, but the directory name is still required.
  std::wstring dir = save_dir_ + L"sub_resource_files";
  EXPECT_TRUE(tab_proxy->SavePage(full_file_name, dir,
                                  SavePackage::SAVE_AS_COMPLETE_HTML));
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  EXPECT_TRUE(WaitForDownloadShelfVisible(browser.get()));
  FilePath expected_file_name =
      FilePath::FromWStringHack(kOverrideTestDir);
  expected_file_name = expected_file_name.Append(kExpectedFileName);
  CheckFile(FilePath::FromWStringHack(full_file_name),
            expected_file_name, true);
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
TEST_F(BrowserEncodingTest, TestEncodingAutoDetect) {
  struct EncodingAutoDetectTestData {
    const wchar_t* test_file_name;   // File name of test data.
    const wchar_t* expected_result;  // File name of expected results.
    const wchar_t* expected_encoding;  // expected encoding.
  };
  const EncodingAutoDetectTestData kTestDatas[] = {
      { L"Big5_with_no_encoding_specified.html",
        L"expected_Big5_saved_from_no_encoding_specified.html",
        L"Big5" },
      { L"gb18030_with_no_encoding_specified.html",
        L"expected_gb18030_saved_from_no_encoding_specified.html",
        L"gb18030" },
      { L"iso-8859-1_with_no_encoding_specified.html",
        L"expected_iso-8859-1_saved_from_no_encoding_specified.html",
        L"ISO-8859-1" },
      { L"ISO-8859-5_with_no_encoding_specified.html",
        L"expected_ISO-8859-5_saved_from_no_encoding_specified.html",
        L"ISO-8859-5" },
      { L"ISO-8859-6_with_no_encoding_specified.html",
        L"expected_ISO-8859-6_saved_from_no_encoding_specified.html",
        L"ISO-8859-6" },
      { L"ISO-8859-7_with_no_encoding_specified.html",
        L"expected_ISO-8859-7_saved_from_no_encoding_specified.html",
        L"ISO-8859-7" },
      { L"ISO-8859-8_with_no_encoding_specified.html",
        L"expected_ISO-8859-8_saved_from_no_encoding_specified.html",
        L"ISO-8859-8-I" },
      { L"KOI8-R_with_no_encoding_specified.html",
        L"expected_KOI8-R_saved_from_no_encoding_specified.html",
        L"KOI8-R" },
      { L"Shift-JIS_with_no_encoding_specified.html",
        L"expected_Shift-JIS_saved_from_no_encoding_specified.html",
        L"Shift_JIS" },
      { L"UTF-8_with_no_encoding_specified.html",
        L"expected_UTF-8_saved_from_no_encoding_specified.html",
        L"UTF-8" },
      { L"windows-949_with_no_encoding_specified.html",
        L"expected_windows-949_saved_from_no_encoding_specified.html",
        L"windows-949" },
      { L"windows-1251_with_no_encoding_specified.html",
        L"expected_windows-1251_saved_from_no_encoding_specified.html",
        L"windows-1251" },
      { L"windows-1254_with_no_encoding_specified.html",
        L"expected_windows-1254_saved_from_no_encoding_specified.html",
        L"windows-1254" },
      { L"windows-1255_with_no_encoding_specified.html",
        L"expected_windows-1255_saved_from_no_encoding_specified.html",
        L"windows-1255" },
      { L"windows-1256_with_no_encoding_specified.html",
        L"expected_windows-1256_saved_from_no_encoding_specified.html",
        L"windows-1256" }
    };
  const wchar_t* const kAutoDetectDir = L"auto_detect";
  // Directory of the files of expected results.
  const wchar_t* const kExpectedResultDir = L"expected_results";

  // Full path of saved file. full_file_name = save_dir_ + file_name[i];
  std::wstring full_saved_file_name;
  // Sub resource directory of saved file.
  std::wstring tmp_save_dir(save_dir_);
  tmp_save_dir += L"sub_resource_files";

  FilePath test_dir_path = FilePath::FromWStringHack(kTestDir);
  test_dir_path = test_dir_path.Append(kAutoDetectDir);

  for (int i = 0;i < arraysize(kTestDatas);i++) {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser.get());

    // Set the default charset to one of encodings not supported by the current
    // auto-detector (Please refer to the above comments) to make sure we
    // incorrectly decode the page. Now we use ISO-8859-4.
    browser->SetStringPreference(prefs::kDefaultCharset, L"ISO-8859-4");
    FilePath test_file_path(test_dir_path);
    test_file_path = test_file_path.Append(kTestDatas[i].test_file_name);
    GURL url =
        URLRequestMockHTTPJob::GetMockUrl(test_file_path.ToWStringHack());
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());
    ASSERT_TRUE(tab->NavigateToURL(url));
    WaitUntilTabCount(1);

    // Disable auto detect if it is on.
    EXPECT_TRUE(
        browser->SetBooleanPreference(prefs::kWebKitUsesUniversalDetector,
                                      false));
    EXPECT_TRUE(tab->Reload());

    // Get the encoding used for the page, it must be the default charset we
    // just set.
    std::wstring encoding;
    EXPECT_TRUE(tab->GetPageCurrentEncoding(&encoding));
    EXPECT_EQ(encoding, L"ISO-8859-4");

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
    full_saved_file_name = save_dir_ + kTestDatas[i].test_file_name;
    // Full path of expect result file.
    FilePath expected_result_file_name =
        FilePath::FromWStringHack(kAutoDetectDir);
    expected_result_file_name =
        expected_result_file_name.Append(kExpectedResultDir);
    expected_result_file_name =
        expected_result_file_name.Append(kTestDatas[i].expected_result);
    EXPECT_TRUE(tab->SavePage(full_saved_file_name, tmp_save_dir,
                              SavePackage::SAVE_AS_COMPLETE_HTML));
    EXPECT_TRUE(WaitForDownloadShelfVisible(browser.get()));
    CheckFile(FilePath::FromWStringHack(full_saved_file_name),
              expected_result_file_name,
              true);
  }
}

