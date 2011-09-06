// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/test/thread_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_switches.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaManager;

// This browser test is aimed towards exercising the FileAPI bindings and
// the actual implementation that lives in the browser side.
class FileSystemBrowserTest : public InProcessBrowserTest {
 public:
  FileSystemBrowserTest() {
    EnableDOMAutomation();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);
  }

  GURL testUrl(const FilePath& file_path) {
    const FilePath kTestDir(FILE_PATH_LITERAL("fileapi"));
    return ui_test_utils::GetTestUrl(kTestDir, file_path);
  }

  void SimpleTest(const GURL& test_url, bool incognito = false) {
    // The test page will perform tests on FileAPI, then navigate to either
    // a #pass or #fail ref.
    Browser* the_browser = incognito ? CreateIncognitoBrowser() : browser();

    LOG(INFO) << "Navigating to URL and blocking.";
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        the_browser, test_url, 2);
    LOG(INFO) << "Navigation done.";
    std::string result = the_browser->GetSelectedTabContents()->GetURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
          the_browser->GetSelectedTabContents()->render_view_host(), L"",
          L"window.domAutomationController.send(getLog())", &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }
};

class FileSystemBrowserTestWithLowQuota : public FileSystemBrowserTest {
 public:
  virtual void SetUpOnMainThread() {
    const int kInitialQuotaKilobytes = 5000;
    const int kTemporaryStorageQuotaMaxSize =
        kInitialQuotaKilobytes * 1024 * QuotaManager::kPerHostTemporaryPortion;
    SetTempQuota(
        kTemporaryStorageQuotaMaxSize, browser()->profile()->GetQuotaManager());
  }

  class SetTempQuotaCallback : public quota::QuotaCallback {
   public:
    void Run(quota::QuotaStatusCode, quota::StorageType, int64) {
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    }

    void RunWithParams(const Tuple3<quota::QuotaStatusCode,
                                    quota::StorageType,
                                    int64>& params) {
      Run(params.a, params.b, params.c);
    }
  };

  static void SetTempQuota(int64 bytes, scoped_refptr<QuotaManager> qm) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
          NewRunnableFunction(&FileSystemBrowserTestWithLowQuota::SetTempQuota,
                              bytes, qm));
      return;
    }
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    qm->SetTemporaryGlobalQuota(bytes, new SetTempQuotaCallback);
    // Don't return until the quota has been set.
    scoped_refptr<base::ThreadTestHelper> helper(
        new base::ThreadTestHelper(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)));
    ASSERT_TRUE(helper->Run());
  }
};

IN_PROC_BROWSER_TEST_F(FileSystemBrowserTest, RequestTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("request_test.html"))));
}

IN_PROC_BROWSER_TEST_F(FileSystemBrowserTest, CreateTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("create_test.html"))));
}

IN_PROC_BROWSER_TEST_F(FileSystemBrowserTestWithLowQuota, QuotaTest) {
  SimpleTest(testUrl(FilePath(FILE_PATH_LITERAL("quota_test.html"))));
}
