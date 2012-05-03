// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/app_non_client_frame_view_aura.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"

class AppNonClientFrameViewAuraTest : public InProcessBrowserTest {
 public:
  AppNonClientFrameViewAuraTest() : InProcessBrowserTest(), app_browser_(NULL) {
  }
  virtual ~AppNonClientFrameViewAuraTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    Browser::CreateParams params = Browser::CreateParams::CreateForApp(
        Browser::TYPE_PANEL,
        std::string("Test"),
        gfx::Rect(),
        browser()->profile());
    params.initial_show_state = ui::SHOW_STATE_MAXIMIZED;
    params.app_type = Browser::APP_TYPE_HOST;
    app_browser_ = Browser::CreateWithParams(params);
    app_browser_->AddBlankTab(true);
    app_browser_->window()->Show();
  }

  AppNonClientFrameViewAura* GetAppFrameView() const {
    BrowserView* browser_view =
        static_cast<BrowserView*>(app_browser_->window());
    BrowserFrame* frame = browser_view->frame();
    return static_cast<AppNonClientFrameViewAura*>(frame->GetFrameView());
  }

  aura::RootWindow* GetRootWindow() const {
    BrowserView* browser_view =
        static_cast<BrowserView*>(app_browser_->window());
    views::Widget* widget = browser_view->GetWidget();
    aura::Window* window =
        static_cast<aura::Window*>(widget->GetNativeWindow());
    return window->GetRootWindow();
  }

  Browser* app_browser() const { return app_browser_; }

 private:
  Browser *app_browser_;
};

// Confirm that we can bring up an app window.
IN_PROC_BROWSER_TEST_F(AppNonClientFrameViewAuraTest, Basic) {
  AppNonClientFrameViewAura* frame_view = GetAppFrameView();
  EXPECT_FALSE(frame_view->IsShowingControls());
}

// Show the controls. We can't test hiding them because it depends on
// MouseWatcher which uses the MessageLoop which is bypassed by EventGenerator.
IN_PROC_BROWSER_TEST_F(AppNonClientFrameViewAuraTest, ShowControls) {
  AppNonClientFrameViewAura* frame_view = GetAppFrameView();
  aura::RootWindow* root_window = GetRootWindow();
  aura::test::EventGenerator eg(root_window, gfx::Point(0, 1));
  EXPECT_FALSE(frame_view->IsShowingControls());
  eg.MoveMouseTo(0, 0);
  EXPECT_TRUE(frame_view->IsShowingControls());
}

// Ensure that we can click the close button when the controls are shown.
// In particular make sure that we can click it on the top pixel of the button.
IN_PROC_BROWSER_TEST_F(AppNonClientFrameViewAuraTest, ClickClose) {
  AppNonClientFrameViewAura* frame_view = GetAppFrameView();
  aura::RootWindow* root_window = GetRootWindow();
  aura::test::EventGenerator eg(root_window, gfx::Point(0, 1));
  EXPECT_FALSE(frame_view->IsShowingControls());

  // Show controls.
  // Don't animate the controls so we'll be able to click on them.
  frame_view->set_animate_controls(false);
  eg.MoveMouseTo(0, 0);
  EXPECT_TRUE(frame_view->IsShowingControls());

  // Click close button.
  eg.MoveMouseTo(root_window->bounds().width() - 1, 0);
  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(app_browser()));
  eg.ClickLeftButton();
  signal.Wait();
  EXPECT_EQ(1,
            static_cast<int>(
                BrowserList::GetBrowserCount(browser()->profile())));
}
