// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/command_line.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dom_distiller {

using ::testing::_;
using namespace switches::reader_mode_heuristics;

namespace {

const char kSimpleArticlePath[] = "/dom_distiller/simple_article.html";
const char kArticlePath[] = "/dom_distiller/og_article.html";
const char kNonArticlePath[] = "/dom_distiller/non_og_article.html";

class Holder {
 public:
  virtual ~Holder() {}
  virtual void OnResult(bool, bool) = 0;
};

class MockDelegate : public Holder {
 public:
  MOCK_METHOD2(OnResult, void(bool, bool));

  base::Callback<void(bool, bool)> GetDelegate() {
    return base::Bind(&MockDelegate::OnResult, base::Unretained(this));
  }
};

}  // namespace

template<const char Option[]>
class DistillablePageUtilsBrowserTestOption : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableDomDistiller);
    command_line->AppendSwitchASCII(switches::kReaderModeHeuristics,
        Option);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    web_contents_ =
        browser()->tab_strip_model()->GetActiveWebContents();
    setDelegate(web_contents_, holder_.GetDelegate());
  }

  void NavigateAndWait(const char* url) {
    GURL article_url(url);
    if (base::StartsWith(url, "/", base::CompareCase::SENSITIVE)) {
      article_url = embedded_test_server()->GetURL(url);
    }

    // This blocks until the navigation has completely finished.
    ui_test_utils::NavigateToURL(browser(), article_url);
    content::WaitForLoadStop(web_contents_);

    // Wait a bit for the message.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
        base::TimeDelta::FromMilliseconds(100));
    content::RunMessageLoop();
  }

  MockDelegate holder_;
  content::WebContents* web_contents_;
};


using DistillablePageUtilsBrowserTestAlways =
    DistillablePageUtilsBrowserTestOption<kAlwaysTrue>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAlways,
                       TestDelegate) {
  // Run twice to make sure the delegate object is still alive.
  for (int i = 0; i < 2; ++i) {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(true, true)).Times(1);
    NavigateAndWait(kSimpleArticlePath);
  }
  // Test pages that we don't care about its distillability.
  {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(_, _)).Times(0);
    NavigateAndWait("about:blank");
  }
}


using DistillablePageUtilsBrowserTestNone =
    DistillablePageUtilsBrowserTestOption<kNone>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestNone,
                       TestDelegate) {
  EXPECT_CALL(holder_, OnResult(_, _)).Times(0);
  NavigateAndWait(kSimpleArticlePath);
}


using DistillablePageUtilsBrowserTestOG =
    DistillablePageUtilsBrowserTestOption<kOGArticle>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestOG,
                       TestDelegate) {
  {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(true, true)).Times(1);
    NavigateAndWait(kArticlePath);
  }
  {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(false, true)).Times(1);
    NavigateAndWait(kNonArticlePath);
  }
}


using DistillablePageUtilsBrowserTestAdaboost =
    DistillablePageUtilsBrowserTestOption<kAdaBoost>;

IN_PROC_BROWSER_TEST_F(DistillablePageUtilsBrowserTestAdaboost,
                       TestDelegate) {
  {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(true, false)).Times(1);
    EXPECT_CALL(holder_, OnResult(true, true)).Times(1);
    NavigateAndWait(kSimpleArticlePath);
  }
  {
    testing::InSequence dummy;
    EXPECT_CALL(holder_, OnResult(false, false)).Times(1);
    EXPECT_CALL(holder_, OnResult(false, true)).Times(1);
    NavigateAndWait(kNonArticlePath);
  }
}

}  // namespace dom_distiller
