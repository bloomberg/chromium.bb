// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_aura.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"

namespace content {

class WebContentsViewAuraTest : public ContentBrowserTest {
 public:
  WebContentsViewAuraTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnableOverscrollHistoryNavigation);
  }

  // Executes the javascript synchronously and makes sure the returned value is
  // freed properly.
  void ExecuteSyncJSFunction(RenderViewHost* rvh, const std::string& jscript) {
    scoped_ptr<base::Value> value(rvh->ExecuteJavascriptAndGetValue(
        string16(), ASCIIToUTF16(jscript)));
  }

  // Starts the test server and navigates to the given url. Sets a large enough
  // size to the root window.  Returns after the navigation to the url is
  // complete.
  void StartTestWithPage(const std::string& url) {
    ASSERT_TRUE(test_server()->Start());
    GURL test_url(test_server()->GetURL(url));
    NavigateToURL(shell(), test_url);
    aura::Window* content = shell()->web_contents()->GetContentNativeView();
    content->GetRootWindow()->SetHostSize(gfx::Size(800, 600));
  }

  void TestOverscrollNavigation(bool touch_handler) {
    ASSERT_NO_FATAL_FAILURE(
        StartTestWithPage("files/overscroll_navigation.html"));
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    NavigationController& controller = web_contents->GetController();
    RenderViewHostImpl* view_host = static_cast<RenderViewHostImpl*>(
        web_contents->GetRenderViewHost());

    EXPECT_FALSE(controller.CanGoBack());
    EXPECT_FALSE(controller.CanGoForward());
    int index = -1;
    scoped_ptr<base::Value> value;
    value.reset(view_host->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("get_current()")));
    ASSERT_TRUE(value->GetAsInteger(&index));
    EXPECT_EQ(0, index);

    if (touch_handler)
      ExecuteSyncJSFunction(view_host, "install_touch_handler()");

    ExecuteSyncJSFunction(view_host, "navigate_next()");
    ExecuteSyncJSFunction(view_host, "navigate_next()");
    value.reset(view_host->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("get_current()")));
    ASSERT_TRUE(value->GetAsInteger(&index));
    EXPECT_EQ(2, index);
    EXPECT_TRUE(controller.CanGoBack());
    EXPECT_FALSE(controller.CanGoForward());

    aura::Window* content = web_contents->GetContentNativeView();
    gfx::Rect bounds = content->GetBoundsInRootWindow();
    aura::test::EventGenerator generator(content->GetRootWindow(), content);

    {
      // Do a swipe-right now. That should navigate backwards.
      string16 expected_title = ASCIIToUTF16("Title: #1");
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          base::TimeDelta::FromMilliseconds(20),
          1);
      string16 actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      value.reset(view_host->ExecuteJavascriptAndGetValue(string16(),
            ASCIIToUTF16("get_current()")));
      ASSERT_TRUE(value->GetAsInteger(&index));
      EXPECT_EQ(1, index);
      EXPECT_TRUE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }

    {
      // Do a fling-right now. That should navigate backwards.
      string16 expected_title = ASCIIToUTF16("Title:");
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          base::TimeDelta::FromMilliseconds(20),
          10);
      string16 actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      value.reset(view_host->ExecuteJavascriptAndGetValue(string16(),
            ASCIIToUTF16("get_current()")));
      ASSERT_TRUE(value->GetAsInteger(&index));
      EXPECT_EQ(0, index);
      EXPECT_FALSE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }

    {
      // Do a swipe-left now. That should navigate forward.
      string16 expected_title = ASCIIToUTF16("Title: #1");
      content::TitleWatcher title_watcher(web_contents, expected_title);
      generator.GestureScrollSequence(
          gfx::Point(bounds.right() - 10, bounds.y() + 10),
          gfx::Point(bounds.x() + 2, bounds.y() + 10),
          base::TimeDelta::FromMilliseconds(20),
          10);
      string16 actual_title = title_watcher.WaitAndGetTitle();
      EXPECT_EQ(expected_title, actual_title);
      value.reset(view_host->ExecuteJavascriptAndGetValue(string16(),
            ASCIIToUTF16("get_current()")));
      ASSERT_TRUE(value->GetAsInteger(&index));
      EXPECT_EQ(1, index);
      EXPECT_TRUE(controller.CanGoBack());
      EXPECT_TRUE(controller.CanGoForward());
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsViewAuraTest);
};

// The tests are disabled on windows since the gesture support in win-aura isn't
// complete yet. See http://crbug.com/157268
#if defined(OS_WIN)
#define MAYBE_OverscrollNavigation DISABLED_OverscrollNavigation
#else
#define MAYBE_OverscrollNavigation OverscrollNavigation
#endif
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_OverscrollNavigation) {
  TestOverscrollNavigation(false);
}

// The tests are disabled on windows since the gesture support in win-aura isn't
// complete yet. See http://crbug.com/157268
#if defined(OS_WIN)
#define MAYBE_OverscrollNavigationWithTouchHandler \
    DISABLED_OverscrollNavigationWithTouchHandler
#else
#define MAYBE_OverscrollNavigationWithTouchHandler \
    OverscrollNavigationWithTouchHandler
#endif
IN_PROC_BROWSER_TEST_F(WebContentsViewAuraTest,
                       MAYBE_OverscrollNavigationWithTouchHandler) {
  TestOverscrollNavigation(true);
}

}  // namespace content
