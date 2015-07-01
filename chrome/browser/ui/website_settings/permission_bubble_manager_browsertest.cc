// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/mock_permission_bubble_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// FakePermissionBubbleView is a MockPermissionBubbleView that overrides Show()
// in order to know how many permissions were requested.
class FakePermissionBubbleView : public MockPermissionBubbleView {
 public:
  FakePermissionBubbleView() : show_count_(0), requests_count_(0) {}

  void Show(const std::vector<PermissionBubbleRequest*>& requests,
            const std::vector<bool>& accept_state) override {
    MockPermissionBubbleView::Show(requests, accept_state);
    show_count_++;
    requests_count_ += requests.size();
  }

  int show_count() const { return show_count_; }
  int requests_count() const { return requests_count_; }

 private:
  int show_count_;
  int requests_count_;
};

class PermissionBubbleManagerBrowserTest : public InProcessBrowserTest {
 public:
  PermissionBubbleManagerBrowserTest() = default;
  ~PermissionBubbleManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    bubble_view_.SetBrowserTest(true);
    GetPermissionBubbleManager()->SetView(&bubble_view_);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePermissionsBubbles);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  PermissionBubbleManager* GetPermissionBubbleManager() {
    return PermissionBubbleManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  void WaitForPermissionBubble() {
    if (bubble_view_.IsVisible())
      return;
    content::RunMessageLoop();
  }

  const FakePermissionBubbleView& bubble_view() const { return bubble_view_; }

 private:
  FakePermissionBubbleView bubble_view_;
};

// Requests before the load event should be bundled into one bubble.
IN_PROC_BROWSER_TEST_F(PermissionBubbleManagerBrowserTest, RequestsBeforeLoad) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_view().show_count());
  EXPECT_EQ(2, bubble_view().requests_count());
}

// Requests before the load should not be bundled with a request after the load.
IN_PROC_BROWSER_TEST_F(PermissionBubbleManagerBrowserTest,
                       RequestsBeforeAfterLoad) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL(
          "/permissions/requests-before-after-load.html"),
      1);
  WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_view().show_count());
  EXPECT_EQ(1, bubble_view().requests_count());
}

}  // anonymous namespace
