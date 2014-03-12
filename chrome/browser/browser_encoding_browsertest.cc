// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/test/net/url_request_mock_http_job.h"

namespace {

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
  { "EUC-KR.html", "EUC-KR" },
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

class SavePackageFinishedObserver : public content::DownloadManager::Observer {
 public:
  SavePackageFinishedObserver(content::DownloadManager* manager,
                              const base::Closure& callback)
      : download_manager_(manager),
        callback_(callback) {
    download_manager_->AddObserver(this);
  }

  virtual ~SavePackageFinishedObserver() {
    if (download_manager_)
      download_manager_->RemoveObserver(this);
  }

  // DownloadManager::Observer:
  virtual void OnSavePackageSuccessfullyFinished(
      content::DownloadManager* manager, content::DownloadItem* item) OVERRIDE {
    callback_.Run();
  }
  virtual void ManagerGoingDown(content::DownloadManager* manager) OVERRIDE {
    download_manager_->RemoveObserver(this);
    download_manager_ = NULL;
  }

 private:
  content::DownloadManager* download_manager_;
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(SavePackageFinishedObserver);
};

}  // namespace

using content::BrowserThread;

static const base::FilePath::CharType* kTestDir =
    FILE_PATH_LITERAL("encoding_tests");

class BrowserEncodingTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<EncodingTestData> {
 protected:
  BrowserEncodingTest() {}

  // Saves the current page and verifies that the output matches the expected
  // result.
  void SaveAndCompare(const char* filename_to_write,
                      const base::FilePath& expected) {
    // Dump the page, the content of dump page should be identical to the
    // expected result file.
    base::FilePath full_file_name = save_dir_.AppendASCII(filename_to_write);
    // We save the page as way of complete HTML file, which requires a directory
    // name to save sub resources in it. Although this test file does not have
    // sub resources, but the directory name is still required.
    scoped_refptr<content::MessageLoopRunner> loop_runner(
        new content::MessageLoopRunner);
    SavePackageFinishedObserver observer(
        content::BrowserContext::GetDownloadManager(browser()->profile()),
        loop_runner->QuitClosure());
    browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
        full_file_name, temp_sub_resource_dir_,
        content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML);
    loop_runner->Run();

    base::FilePath expected_file_name = ui_test_utils::GetTestFilePath(
        base::FilePath(kTestDir), expected);

    EXPECT_TRUE(base::ContentsEqual(full_file_name, expected_file_name));
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    save_dir_ = temp_dir_.path();
    temp_sub_resource_dir_ = save_dir_.AppendASCII("sub_resource_files");

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath save_dir_;
  base::FilePath temp_sub_resource_dir_;
};

// TODO(jnd): 1. Some encodings are missing here. It'll be added later. See
// http://crbug.com/13306.
// 2. Add more files with multiple encoding name variants for each canonical
// encoding name). Webkit layout tests cover some, but testing in the UI test is
// also necessary.
IN_PROC_BROWSER_TEST_P(BrowserEncodingTest, TestEncodingAliasMapping) {
  const char* const kAliasTestDir = "alias_mapping";

  base::FilePath test_dir_path = base::FilePath(kTestDir).AppendASCII(
      kAliasTestDir);
  base::FilePath test_file_path(test_dir_path);
  test_file_path = test_file_path.AppendASCII(
      GetParam().file_name);

  GURL url = content::URLRequestMockHTTPJob::GetMockUrl(test_file_path);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(GetParam().encoding_name,
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetEncoding());
}

INSTANTIATE_TEST_CASE_P(EncodingAliases,
                        BrowserEncodingTest,
                        testing::ValuesIn(kEncodingTestDatas));

// Marked as flaky: see  http://crbug.com/44668
IN_PROC_BROWSER_TEST_F(BrowserEncodingTest, TestOverrideEncoding) {
  const char* const kTestFileName = "gb18030_with_iso88591_meta.html";
  const char* const kExpectedFileName =
      "expected_gb18030_saved_from_iso88591_meta.html";
  const char* const kOverrideTestDir = "user_override";

  base::FilePath test_dir_path =
      base::FilePath(kTestDir).AppendASCII(kOverrideTestDir);
  test_dir_path = test_dir_path.AppendASCII(kTestFileName);
  GURL url = content::URLRequestMockHTTPJob::GetMockUrl(test_dir_path);
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ("ISO-8859-1", web_contents->GetEncoding());

  // Override the encoding to "gb18030".
  const std::string selected_encoding =
      CharacterEncoding::GetCanonicalEncodingNameByAliasName("gb18030");
  content::TestNavigationObserver navigation_observer(web_contents);
  web_contents->SetOverrideEncoding(selected_encoding);
  navigation_observer.Wait();
  EXPECT_EQ("gb18030", web_contents->GetEncoding());

  base::FilePath expected_filename =
      base::FilePath().AppendASCII(kOverrideTestDir).AppendASCII(
          kExpectedFileName);
  SaveAndCompare(kTestFileName, expected_filename);
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
//
// This test fails frequently on the win_rel trybot. See http://crbug.com/122053
// It also times out frequently on Mac dbg. See http://crbug.com/351325
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_TestEncodingAutoDetect DISABLED_TestEncodingAutoDetect
#else
#define MAYBE_TestEncodingAutoDetect TestEncodingAutoDetect
#endif
// TODO(phajdan.jr): See if fix for http://crbug.com/122053 would help here.
IN_PROC_BROWSER_TEST_F(BrowserEncodingTest, MAYBE_TestEncodingAutoDetect) {
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
      { "EUC-KR_with_no_encoding_specified.html",
        "expected_EUC-KR_saved_from_no_encoding_specified.html",
        "EUC-KR" },
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

  base::FilePath test_dir_path =
      base::FilePath(kTestDir).AppendASCII(kAutoDetectDir);

  // Set the default charset to one of encodings not supported by the current
  // auto-detector (Please refer to the above comments) to make sure we
  // incorrectly decode the page. Now we use ISO-8859-4.
  browser()->profile()->GetPrefs()->SetString(prefs::kDefaultCharset,
                                              "ISO-8859-4");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestDatas); ++i) {
    // Disable auto detect if it is on.
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kWebKitUsesUniversalDetector, false);

    base::FilePath test_file_path(test_dir_path);
    test_file_path = test_file_path.AppendASCII(kTestDatas[i].test_file_name);
    GURL url = content::URLRequestMockHTTPJob::GetMockUrl(test_file_path);
    ui_test_utils::NavigateToURL(browser(), url);

    // Get the encoding used for the page, it must be the default charset we
    // just set.
    EXPECT_EQ("ISO-8859-4", web_contents->GetEncoding());

    // Enable the encoding auto detection.
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kWebKitUsesUniversalDetector, true);

    content::TestNavigationObserver observer(web_contents);
    chrome::Reload(browser(), CURRENT_TAB);
    observer.Wait();

    // Re-get the encoding of page. It should return the real encoding now.
    EXPECT_EQ(kTestDatas[i].expected_encoding, web_contents->GetEncoding());

    // Dump the page, the content of dump page should be equal with our expect
    // result file.
    base::FilePath expected_result_file_name =
        base::FilePath().AppendASCII(kAutoDetectDir).
        AppendASCII(kExpectedResultDir).
        AppendASCII(kTestDatas[i].expected_result);
    SaveAndCompare(kTestDatas[i].test_file_name, expected_result_file_name);
  }
}
