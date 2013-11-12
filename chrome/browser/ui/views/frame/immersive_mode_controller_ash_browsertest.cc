// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/aura/window.h"

// TODO(jamescook): If immersive mode becomes popular on CrOS, consider porting
// it to other Aura platforms (win_aura, linux_aura).  http://crbug.com/163931
#if defined(OS_CHROMEOS)

class ImmersiveModeControllerAshTest : public InProcessBrowserTest {
 public:
  ImmersiveModeControllerAshTest() : browser_view_(NULL), controller_(NULL) {}
  virtual ~ImmersiveModeControllerAshTest() {}

  BrowserView* browser_view() { return browser_view_; }
  ImmersiveModeController* controller() { return controller_; }

  // content::BrowserTestBase overrides:
  virtual void SetUpOnMainThread() OVERRIDE {
    browser_view_ = static_cast<BrowserView*>(browser()->window());
    controller_ = browser_view_->immersive_mode_controller();
    controller_->SetupForTest();
  }

 private:
  BrowserView* browser_view_;
  ImmersiveModeController* controller_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeControllerAshTest);
};

// Validate top container touch insets are being updated at the correct time in
// immersive mode.
IN_PROC_BROWSER_TEST_F(ImmersiveModeControllerAshTest,
                       ImmersiveTopContainerInsets) {
  content::WebContents* contents = browser_view()->GetActiveWebContents();
  aura::Window* window = contents->GetView()->GetContentNativeView();

  // Turning immersive mode on sets positive top touch insets on the render view
  // window.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_TRUE(browser_view()->IsFullscreen());
  ASSERT_TRUE(controller()->IsEnabled());
  ASSERT_FALSE(controller()->IsRevealed());
  EXPECT_TRUE(window->hit_test_bounds_override_outer_touch().top() > 0);

  // Trigger a reveal resets insets as now the touch target for the top
  // container is large enough.
  scoped_ptr<ImmersiveRevealedLock> lock(controller()->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(controller()->IsRevealed());
  EXPECT_TRUE(window->hit_test_bounds_override_outer_touch().top() == 0);

  // End reveal by moving the mouse off the top-of-window views. We
  // should see the top insets being positive again to allow a bigger touch
  // target.
  lock.reset();
  EXPECT_FALSE(controller()->IsRevealed());
  EXPECT_TRUE(window->hit_test_bounds_override_outer_touch().top() > 0);

  // Disabling immersive mode resets the top touch insets to 0.
  chrome::ToggleFullscreenMode(browser());
  ASSERT_FALSE(browser_view()->IsFullscreen());
  ASSERT_FALSE(controller()->IsEnabled());
  EXPECT_TRUE(window->hit_test_bounds_override_outer_touch().top() == 0);
}

#endif  // defined(OS_CHROMEOS)
