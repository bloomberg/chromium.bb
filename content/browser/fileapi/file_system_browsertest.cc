// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/thread_test_helper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "storage/browser/fileapi/file_system_features.h"
#include "storage/browser/quota/quota_manager.h"

using storage::QuotaManager;

namespace content {

// This browser test is aimed towards exercising the File System API bindings
// and the actual implementation that lives in the browser side.
class FileSystemBrowserTest : public ContentBrowserTest,
                              public testing::WithParamInterface<bool> {
 public:
  FileSystemBrowserTest() {
    feature_list_.InitAndEnableFeature(
        storage::features::kEnableFilesystemInIncognito);
  }

  void SimpleTest(const GURL& test_url) {
    const bool incognito = GetParam();
    // The test page will perform tests on FileAPI, then navigate to either
    // a #pass or #fail ref.
    Shell* the_browser = incognito ? CreateOffTheRecordBrowser() : shell();

    VLOG(0) << "Navigating to URL and blocking.";
    NavigateToURLBlockUntilNavigationsComplete(the_browser, test_url, 2);
    VLOG(0) << "Navigation done.";
    std::string result =
        the_browser->web_contents()->GetLastCommittedURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ExecuteScriptAndExtractString(
          the_browser, "window.domAutomationController.send(getLog())",
          &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(, FileSystemBrowserTest, ::testing::Bool());

class FileSystemBrowserTestWithLowQuota : public FileSystemBrowserTest {
 public:
  void SetUpOnMainThread() override {
    SetLowQuota(BrowserContext::GetDefaultStoragePartition(
                    shell()->web_contents()->GetBrowserContext())
                    ->GetQuotaManager());
  }

  static void SetLowQuota(scoped_refptr<QuotaManager> qm) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&FileSystemBrowserTestWithLowQuota::SetLowQuota, qm));
      return;
    }
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // These sizes must correspond with expectations in html and js.
    const int kMeg = 1000 * 1024;
    storage::QuotaSettings settings;
    settings.pool_size = 25 * kMeg;
    settings.per_host_quota = 5 * kMeg;
    settings.must_remain_available = 100 * kMeg;
    settings.refresh_interval = base::TimeDelta::Max();
    qm->SetQuotaSettings(settings);
  }
};

// TODO(https://crbug.com/93417): Expand the test after updating quota
// managenent for in-memory implementation.
INSTANTIATE_TEST_SUITE_P(,
                         FileSystemBrowserTestWithLowQuota,
                         ::testing::Values(false));

IN_PROC_BROWSER_TEST_P(FileSystemBrowserTest, RequestTest) {
  SimpleTest(GetTestUrl("fileapi", "request_test.html"));
}

IN_PROC_BROWSER_TEST_P(FileSystemBrowserTest, CreateTest) {
  SimpleTest(GetTestUrl("fileapi", "create_test.html"));
}

IN_PROC_BROWSER_TEST_P(FileSystemBrowserTestWithLowQuota, QuotaTest) {
  SimpleTest(GetTestUrl("fileapi", "quota_test.html"));
}

}  // namespace content
