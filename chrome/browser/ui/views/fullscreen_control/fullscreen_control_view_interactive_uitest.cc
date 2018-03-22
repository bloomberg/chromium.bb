// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"
#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "url/gurl.h"

class FullscreenControlViewTest : public InProcessBrowserTest {
 public:
  FullscreenControlViewTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableExperimentalFullscreenExitUI);
  }

 protected:
  FullscreenControlHost* GetFullscreenControlHost() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->GetFullscreenControlHost();
  }

  FullscreenControlView* GetFullscreenControlView() {
    return GetFullscreenControlHost()->fullscreen_control_view();
  }

  views::Button* GetFullscreenExitButton() {
    return GetFullscreenControlView()->exit_fullscreen_button();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FullscreenControlViewTest);
};

#if defined(OS_MACOSX)
// Entering fullscreen is flaky on Mac: http://crbug.com/824517
#define MAYBE_MouseExitFullscreen DISABLED_MouseExitFullscreen
#else
#define MAYBE_MouseExitFullscreen MouseExitFullscreen
#endif
IN_PROC_BROWSER_TEST_F(FullscreenControlViewTest, MAYBE_MouseExitFullscreen) {
  GURL blank_url("about:blank");
  ui_test_utils::NavigateToURL(browser(), blank_url);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  browser_view->EnterFullscreen(
      blank_url, ExclusiveAccessBubbleType::EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE);

  ASSERT_TRUE(browser_view->IsFullscreen());

  // Simulate moving the mouse to the top of the screen, which should show the
  // fullscreen exit UI.
  FullscreenControlHost* host = GetFullscreenControlHost();
  EXPECT_FALSE(host->IsVisible());
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED, gfx::Point(1, 1), gfx::Point(),
                            base::TimeTicks(), 0, 0);
  host->OnMouseEvent(&mouse_move);
  EXPECT_TRUE(host->IsVisible());

  // Simulate clicking on the fullscreen exit button, which should cause the
  // browser to exit fullscreen and hide the fullscreen exit control.
  ui::MouseEvent mouse_click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  GetFullscreenControlView()->ButtonPressed(GetFullscreenExitButton(),
                                            mouse_click);

  EXPECT_FALSE(host->IsVisible());
  EXPECT_FALSE(browser_view->IsFullscreen());
}
