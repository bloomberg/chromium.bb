// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/javascript_test_util.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "views/event.h"

#if defined(OS_MACOSX)
// window->GetViewBounds is not implemented
// window->SimulateOSMouseMove is not implemented
// http://code.google.com/p/chromium/issues/detail?id=26102
#define MAYBE_TestOnMouseOut DISABLED_TestOnMouseOut
#elif defined(OS_WIN)
// Test succeeds locally, flaky on trybot
// http://code.google.com/p/chromium/issues/detail?id=26349
#define MAYBE_TestOnMouseOut DISABLED_TestOnMouseOut
#elif defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
// Test is flaky on linux/views build.
// http://crbug.com/28808.
#define MAYBE_TestOnMouseOut FLAKY_TestOnMouseOut
#endif

namespace {

class MouseLeaveTest : public UITest {
 public:
  MouseLeaveTest() {
    dom_automation_enabled_ = true;
    show_window_ = true;
  }

  DISALLOW_COPY_AND_ASSIGN(MouseLeaveTest);
};

TEST_F(MouseLeaveTest, MAYBE_TestOnMouseOut) {
  GURL test_url = GetTestUrl(L"", L"mouseleave.html");

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  scoped_refptr<BrowserProxy> browser = automation()->GetBrowserWindow(0);
  scoped_refptr<WindowProxy> window = browser->GetWindow();

  gfx::Rect tab_view_bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER, &tab_view_bounds,
                                    true));
  gfx::Point in_content_point(
      tab_view_bounds.x() + tab_view_bounds.width() / 2,
      tab_view_bounds.y() + 10);
  gfx::Point above_content_point(
      tab_view_bounds.x() + tab_view_bounds.width() / 2,
      tab_view_bounds.y() - 2);

  // Start by moving the point just above the content.
  ASSERT_TRUE(window->SimulateOSMouseMove(above_content_point));

  // Navigate to the test html page.
  tab->NavigateToURL(test_url);

  const int timeout_ms = 5 * action_max_timeout_ms();
  const int check_interval_ms = action_max_timeout_ms() / 10;

  // Wait for the onload() handler to complete so we can do the
  // next part of the test.
  ASSERT_TRUE(WaitUntilCookieValue(
      tab.get(), test_url, "__state", check_interval_ms, timeout_ms,
      "initial"));

  // Move the cursor to the top-center of the content, which will trigger
  // a javascript onMouseOver event.
  ASSERT_TRUE(window->SimulateOSMouseMove(in_content_point));

  // Wait on the correct intermediate value of the cookie.
  ASSERT_TRUE(WaitUntilCookieValue(
      tab.get(), test_url, "__state", check_interval_ms, timeout_ms,
      "initial,entered"));

  // Move the cursor above the content again, which should trigger
  // a javascript onMouseOut event.
  ASSERT_TRUE(window->SimulateOSMouseMove(above_content_point));

  // Wait on the correct final value of the cookie.
  ASSERT_TRUE(WaitUntilCookieValue(
      tab.get(), test_url, "__state", check_interval_ms, timeout_ms,
      "initial,entered,left"));
}

}  // namespace
