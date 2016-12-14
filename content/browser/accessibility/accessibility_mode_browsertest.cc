// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const char kMinimalPageDataURL[] =
    "data:text/html,<html><head></head><body>Hello, world</body></html>";

class AccessibilityModeTest : public ContentBrowserTest {
 protected:
  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void ExpectBrowserAccessibilityManager(bool expect_bam,
                                         std::string message = "") {
    if (expect_bam) {
      EXPECT_NE(
          (BrowserAccessibilityManager*)NULL,
          web_contents()->GetRootBrowserAccessibilityManager()) << message;
    } else {
      EXPECT_EQ(
          (BrowserAccessibilityManager*)NULL,
          web_contents()->GetRootBrowserAccessibilityManager()) << message;
    }
  }

  bool ShouldBeBrowserAccessibilityManager(AccessibilityMode mode) {
    switch (mode) {
      case AccessibilityModeOff:
      case ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY:
        return false;
      case ACCESSIBILITY_MODE_COMPLETE:
        return true;
      default:
        NOTREACHED();
    }
    return false;
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeOff) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));

  EXPECT_EQ(AccessibilityModeOff, web_contents()->GetAccessibilityMode());
  ExpectBrowserAccessibilityManager(
      ShouldBeBrowserAccessibilityManager(AccessibilityModeOff));
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, ACCESSIBILITY_MODE_COMPLETE) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));
  ASSERT_EQ(AccessibilityModeOff, web_contents()->GetAccessibilityMode());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ACCESSIBILITY_MODE_COMPLETE);
  EXPECT_EQ(ACCESSIBILITY_MODE_COMPLETE,
            web_contents()->GetAccessibilityMode());
  waiter.WaitForNotification();
  ExpectBrowserAccessibilityManager(
      ShouldBeBrowserAccessibilityManager(ACCESSIBILITY_MODE_COMPLETE));
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest,
                       ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));
  ASSERT_EQ(AccessibilityModeOff, web_contents()->GetAccessibilityMode());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY);
  EXPECT_EQ(ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY,
            web_contents()->GetAccessibilityMode());
  waiter.WaitForNotification();
  // No BrowserAccessibilityManager expected for
  // ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY
  ExpectBrowserAccessibilityManager(ShouldBeBrowserAccessibilityManager(
      ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY));
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AddingModes) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY);
  EXPECT_EQ(ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY,
            web_contents()->GetAccessibilityMode());
  waiter.WaitForNotification();
  ExpectBrowserAccessibilityManager(
      ShouldBeBrowserAccessibilityManager(ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY),
      "Should be no BrowserAccessibilityManager "
      "for ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY");

  AccessibilityNotificationWaiter waiter2(shell()->web_contents());
  web_contents()->AddAccessibilityMode(ACCESSIBILITY_MODE_COMPLETE);
  EXPECT_EQ(ACCESSIBILITY_MODE_COMPLETE,
            web_contents()->GetAccessibilityMode());
  waiter2.WaitForNotification();
  ExpectBrowserAccessibilityManager(
      ShouldBeBrowserAccessibilityManager(ACCESSIBILITY_MODE_COMPLETE),
      "Should be a BrowserAccessibilityManager "
      "for ACCESSIBILITY_MODE_COMPLETE");
}

}  // namespace content
