// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::URLRequestMockHTTPJob;


namespace {

// Tests that the NPAPI unauthorized plugin infobar is:
// (1) Shown when the Finch trial is set to "Enabled"
// (2) Not shown when the Finch trial is set to "Disabled"
// TODO(cthomp): Remove/simplify when Finch trial is completed crbug.com/381944
class UnauthorizedPluginInfoBarBrowserTest : public InProcessBrowserTest {
 protected:
  UnauthorizedPluginInfoBarBrowserTest() {}

  // Makes URLRequestMockHTTPJobs serve data from content::DIR_TEST_DATA
  // instead of chrome::DIR_TEST_DATA.
  void ServeContentTestData() {
    base::FilePath root_http;
    PathService::Get(content::DIR_TEST_DATA, &root_http);
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO, FROM_HERE,
        base::Bind(URLRequestMockHTTPJob::AddUrlHandler, root_http),
        runner->QuitClosure());
    runner->Run();
  }

 public:
  // Verifies that the test page exists (only present with src-internal)
  bool TestPageExists() {
    return base::PathExists(ui_test_utils::GetTestFilePath(
        base::FilePath(FILE_PATH_LITERAL("plugin")),
        base::FilePath(FILE_PATH_LITERAL("quicktime.html"))));
  }

  void InfoBarCountTest(size_t number_infobars_expected,
                        Browser* browser) {
    ServeContentTestData();
    content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(contents);
    InfoBarService* infobar_service = InfoBarService::FromWebContents(contents);
    ASSERT_TRUE(infobar_service);
    EXPECT_EQ(0u, infobar_service->infobar_count());

    base::FilePath path(FILE_PATH_LITERAL("plugin/quicktime.html"));
    GURL url(URLRequestMockHTTPJob::GetMockUrl(path));
    ui_test_utils::NavigateToURL(browser, url);
    ASSERT_EQ(number_infobars_expected, infobar_service->infobar_count());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UnauthorizedPluginInfoBarBrowserTest);
};

// When "UnauthorizedPluginInfoBar" is "Enabled", infobar for NPAPI plugin
// should be shown.
IN_PROC_BROWSER_TEST_F(UnauthorizedPluginInfoBarBrowserTest, InfobarEnabled) {
  if (!TestPageExists()) {
    LOG(INFO) <<
        "Test skipped because plugin/quicktime.html test file wasn't found.";
    return;
  }
  base::FieldTrialList::CreateTrialsFromString(
      "UnauthorizedPluginInfoBar/Enabled/",
      base::FieldTrialList::ACTIVATE_TRIALS,
       std::set<std::string>());
  InfoBarCountTest(1u, browser());
}

// When "UnauthorizedPluginInfoBar" is "Disabled", infobar for NPAPI plugin
// should not be shown.
IN_PROC_BROWSER_TEST_F(UnauthorizedPluginInfoBarBrowserTest, InfobarDisabled) {
  if (!TestPageExists()) {
    LOG(INFO) <<
        "Test skipped because plugin/quicktime.html test file wasn't found.";
    return;
  }
  base::FieldTrialList::CreateTrialsFromString(
      "UnauthorizedPluginInfoBar/Disabled/",
      base::FieldTrialList::ACTIVATE_TRIALS,
      std::set<std::string>());
  InfoBarCountTest(0u, browser());
}

}  // namespace
