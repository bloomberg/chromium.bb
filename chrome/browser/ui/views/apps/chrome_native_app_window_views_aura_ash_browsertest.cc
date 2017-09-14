// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura_ash.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/base/ui_base_types.h"

class ChromeNativeAppWindowViewsAuraAshBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  ChromeNativeAppWindowViewsAuraAshBrowserTest() = default;
  ~ChromeNativeAppWindowViewsAuraAshBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViewsAuraAshBrowserTest);
};

// Verify that immersive mode is enabled or disabled as expected.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       ImmersiveWorkFlow) {
  extensions::AppWindow* app_window = CreateTestAppWindow("{}");
  auto* window = static_cast<ChromeNativeAppWindowViewsAuraAsh*>(
      GetNativeAppWindowForAppWindow(app_window));
  ASSERT_TRUE(window != nullptr);
  ASSERT_TRUE(window->immersive_fullscreen_controller_.get() != nullptr);
  EXPECT_FALSE(window->immersive_fullscreen_controller_->IsEnabled());

  // Verify that when fullscreen is toggled on, immersive mode is enabled and
  // that when fullscreen is toggled off, immersive mode is disabled.
  window->SetFullscreen(extensions::AppWindow::FULLSCREEN_TYPE_OS);
  EXPECT_TRUE(window->immersive_fullscreen_controller_->IsEnabled());
  window->SetFullscreen(extensions::AppWindow::FULLSCREEN_TYPE_NONE);
  EXPECT_FALSE(window->immersive_fullscreen_controller_->IsEnabled());

  // Verify that since the auto hide title bars in tablet mode feature turned
  // on, immersive mode is enabled once tablet mode is entered, and disabled
  // once tablet mode is exited.
  ash::Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(
      true);
  EXPECT_TRUE(window->immersive_fullscreen_controller_->IsEnabled());
  ash::Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(
      false);
  EXPECT_FALSE(window->immersive_fullscreen_controller_->IsEnabled());

  // Verify that the window was fullscreened before entering tablet mode, it
  // will remain fullscreened after exiting tablet mode.
  window->SetFullscreen(extensions::AppWindow::FULLSCREEN_TYPE_OS);
  EXPECT_TRUE(window->immersive_fullscreen_controller_->IsEnabled());
  ash::Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(
      true);
  EXPECT_TRUE(window->immersive_fullscreen_controller_->IsEnabled());
  ash::Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(
      false);
  EXPECT_TRUE(window->immersive_fullscreen_controller_->IsEnabled());

  CloseAppWindow(app_window);
}

// Verifies that apps in immersive fullscreen will have a restore state of
// maximized.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       ImmersiveModeFullscreenRestoreType) {
  extensions::AppWindow* app_window = CreateTestAppWindow("{}");
  auto* window = static_cast<ChromeNativeAppWindowViewsAuraAsh*>(
      GetNativeAppWindowForAppWindow(app_window));
  ASSERT_TRUE(window != nullptr);
  ASSERT_TRUE(window->immersive_fullscreen_controller_.get() != nullptr);

  window->SetFullscreen(extensions::AppWindow::FULLSCREEN_TYPE_OS);
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window->GetRestoredState());
  ash::Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(
      true);
  EXPECT_TRUE(window->IsFullscreen());
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window->GetRestoredState());
  ash::Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(
      false);
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window->GetRestoredState());

  CloseAppWindow(app_window);
}
