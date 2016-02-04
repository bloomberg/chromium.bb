// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/mock_permission_bubble_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

class PermissionBubbleManagerBrowserTest : public InProcessBrowserTest {
 public:
  PermissionBubbleManagerBrowserTest() = default;
  ~PermissionBubbleManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    PermissionBubbleManager* manager = GetPermissionBubbleManager();
    mock_permission_bubble_factory_.reset(
        new MockPermissionBubbleFactory(true, manager));
    manager->DisplayPendingRequests();
  }

  void TearDownOnMainThread() override {
    mock_permission_bubble_factory_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  PermissionBubbleManager* GetPermissionBubbleManager() {
    return PermissionBubbleManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  void WaitForPermissionBubble() {
    if (bubble_factory()->is_visible())
      return;
    content::RunMessageLoop();
  }

  MockPermissionBubbleFactory* bubble_factory() {
    return mock_permission_bubble_factory_.get();
  }

 private:
  scoped_ptr<MockPermissionBubbleFactory> mock_permission_bubble_factory_;
};

// Requests before the load event should be bundled into one bubble.
// http://crbug.com/512849 flaky
IN_PROC_BROWSER_TEST_F(PermissionBubbleManagerBrowserTest,
                       DISABLED_RequestsBeforeLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(2, bubble_factory()->total_request_count());
}

// Requests before the load should not be bundled with a request after the load.
IN_PROC_BROWSER_TEST_F(PermissionBubbleManagerBrowserTest,
                       RequestsBeforeAfterLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL(
          "/permissions/requests-before-after-load.html"),
      1);
  WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->total_request_count());
}

// Navigating twice to the same URL should be equivalent to refresh. This means
// showing the bubbles twice.
// http://crbug.com/512849 flaky
#if defined(OS_WIN)
#define MAYBE_NavTwice DISABLED_NavTwice
#else
#define MAYBE_NavTwice NavTwice
#endif
IN_PROC_BROWSER_TEST_F(PermissionBubbleManagerBrowserTest, MAYBE_NavTwice) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  WaitForPermissionBubble();

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  WaitForPermissionBubble();

  EXPECT_EQ(2, bubble_factory()->show_count());
  EXPECT_EQ(4, bubble_factory()->total_request_count());
}

// Navigating twice to the same URL with a hash should be navigation within the
// page. This means the bubble is only shown once.
// http://crbug.com/512849 flaky
#if defined(OS_WIN)
#define MAYBE_NavTwiceWithHash DISABLED_NavTwiceWithHash
#else
#define MAYBE_NavTwiceWithHash NavTwiceWithHash
#endif
IN_PROC_BROWSER_TEST_F(PermissionBubbleManagerBrowserTest,
                       MAYBE_NavTwiceWithHash) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  WaitForPermissionBubble();

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL(
          "/permissions/requests-before-load.html#0"),
      1);
  WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(2, bubble_factory()->total_request_count());
}

// Bubble requests should be shown after in-page navigation.
IN_PROC_BROWSER_TEST_F(PermissionBubbleManagerBrowserTest, InPageNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/empty.html"),
      1);

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/empty.html#0"),
      1);

  // Request 'geolocation' permission.
  ExecuteScriptAndGetValue(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      "navigator.geolocation.getCurrentPosition(function(){});");
  WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->total_request_count());
}

}  // anonymous namespace
