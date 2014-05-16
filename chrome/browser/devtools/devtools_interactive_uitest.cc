// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

using content::RenderViewHost;
using content::WebContents;

namespace {
const char kTouchEmulationPage[] = "files/devtools/touch_emulation.html";
}  // namespace

class DevToolsInteractiveTest : public InProcessBrowserTest {
 public:
  DevToolsInteractiveTest()
      : devtools_window_(NULL) {}

 protected:
  void LoadTestPage(const std::string& test_page) {
    GURL url = test_server()->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void OpenDevToolsWindow(const std::string& test_page, bool is_docked) {
    ASSERT_TRUE(test_server()->Start());
    LoadTestPage(test_page);

    devtools_window_ = DevToolsWindow::OpenDevToolsWindowForTest(
        GetInspectedTab()->GetRenderViewHost(), is_docked);
    ui_test_utils::WaitUntilDevToolsWindowLoaded(devtools_window_);
  }

  WebContents* GetInspectedTab() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void CloseDevToolsWindow() {
    content::DevToolsManager* devtools_manager =
        content::DevToolsManager::GetInstance();
    content::WindowedNotificationObserver close_observer(
        content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::Source<content::WebContents>(
            devtools_window_->web_contents()));
    devtools_manager->CloseAllClientHosts();
    close_observer.Wait();
  }

  DevToolsWindow* devtools_window_;
};

// SendMouseMoseSync only reliably works on Linux.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_TouchEmulation TouchEmulation
#else
#define MAYBE_TouchEmulation DISABLED_TouchEmulation
#endif

IN_PROC_BROWSER_TEST_F(DevToolsInteractiveTest, MAYBE_TouchEmulation) {
  OpenDevToolsWindow(kTouchEmulationPage, false);
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  std::string result;

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      devtools_window_->web_contents()->GetRenderViewHost(),
      "uiTests.runTest('enableTouchEmulation')",
      &result));
  EXPECT_EQ("[OK]", result);

  // Clear dirty events before touch emulation.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetInspectedTab()->GetRenderViewHost(),
      "getEventNames(0)",
      &result));

  gfx::Rect bounds = GetInspectedTab()->GetContainerBounds();
  gfx::Point position = bounds.origin();

  // Simple touch sequence.
  position.Offset(10, 10);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(position));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
      ui_controls::LEFT, ui_controls::DOWN));
  position.Offset(30, 0);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(position));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
      ui_controls::LEFT, ui_controls::UP));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetInspectedTab()->GetRenderViewHost(),
      "getEventNames(3)",
      &result));
  EXPECT_EQ("touchstart touchmove touchend", result);

  // If mouse is not pressed - no touches.
  position.Offset(-30, 0);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(position));
  position.Offset(30, 0);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(position));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetInspectedTab()->GetRenderViewHost(),
      "getEventNames(0)",
      &result));
  EXPECT_EQ("", result);

  // Don't prevent default to get click sequence.
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetInspectedTab()->GetRenderViewHost(),
      "preventDefault = false; window.domAutomationController.send('OK')",
      &result));
  // Click.
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
      ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(ui_test_utils::SendMouseEventsSync(
      ui_controls::LEFT, ui_controls::UP));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetInspectedTab()->GetRenderViewHost(),
      "getEventNames(6)",
      &result));
  EXPECT_EQ(
      "touchstart touchend mousemove mousedown mouseup click",
      result);

  CloseDevToolsWindow();

  // Touch emulation is off - mouse events should come.
  position.Offset(-30, 0);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(position));
  position.Offset(30, 0);
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(position));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetInspectedTab()->GetRenderViewHost(),
      "getEventNames(2)",
      &result));
  EXPECT_EQ("mousemove mousemove", result);

  // Ensure no extra events.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
  content::RunAllPendingInMessageLoop();
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetInspectedTab()->GetRenderViewHost(),
      "getEventNames(0)",
      &result));
  EXPECT_EQ("", result);
}
