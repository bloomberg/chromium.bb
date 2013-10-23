// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "apps/shell_window.h"
#include "apps/ui/native_app_window.h"
#include "ash/accelerators/accelerator_commands.h"
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/aura/client/aura_constants.h"

namespace {

// Returns true if |window| is in immersive fullscreen. Infer whether |window|
// is in immersive fullscreen based on whether kFullscreenUsesMinimalChromeKey
// is set for |window| because DEPS does not allow the test to use BrowserView.
// (This is not quite right because if a window is in both immersive browser
// fullscreen and in tab fullscreen, kFullScreenUsesMinimalChromeKey will
// not be set).
bool IsInImmersiveFullscreen(BrowserWindow* browser_window) {
  return browser_window->IsFullscreen() &&
      browser_window->GetNativeWindow()->GetProperty(
          ash::internal::kFullscreenUsesMinimalChromeKey);
}

}  // namespace

typedef InProcessBrowserTest ChromeShellDelegateBrowserTest;

// TODO(oshima): Move these tests to ash once ToggleFullscreen is moved
// to ash. crbug.com/309837.

// Confirm that toggling window miximized works properly
IN_PROC_BROWSER_TEST_F(ChromeShellDelegateBrowserTest, ToggleMaximized) {
  ash::ShellDelegate* shell_delegate = ash::Shell::GetInstance()->delegate();
  ASSERT_TRUE(shell_delegate);
  ash::wm::WindowState* window_state = ash::wm::GetActiveWindowState();
  ASSERT_TRUE(window_state);

  // When not in fullscreen, accelerators::ToggleMaximized toggles Maximized.
  EXPECT_FALSE(window_state->IsMaximized());
  ash::accelerators::ToggleMaximized();
  EXPECT_TRUE(window_state->IsMaximized());
  ash::accelerators::ToggleMaximized();
  EXPECT_FALSE(window_state->IsMaximized());

  // When in fullscreen accelerators::ToggleMaximized gets out of fullscreen.
  EXPECT_FALSE(window_state->IsFullscreen());
  Browser* browser = chrome::FindBrowserWithWindow(window_state->window());
  ASSERT_TRUE(browser);
  chrome::ToggleFullscreenMode(browser);
  EXPECT_TRUE(window_state->IsFullscreen());
  ash::accelerators::ToggleMaximized();
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  ash::accelerators::ToggleMaximized();
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsMaximized());
}

// Confirm that toggling window fullscren works properly.
IN_PROC_BROWSER_TEST_F(ChromeShellDelegateBrowserTest, ToggleFullscreen) {
  ash::ShellDelegate* shell_delegate = ash::Shell::GetInstance()->delegate();
  ASSERT_TRUE(shell_delegate);

  // 1) ToggleFullscreen() should toggle whether a tabbed browser window is in
  // immersive fullscreen.
  ASSERT_TRUE(browser()->is_type_tabbed());
  BrowserWindow* browser_window = browser()->window();
  ASSERT_TRUE(browser_window->IsActive());
  EXPECT_FALSE(browser_window->IsMaximized());
  EXPECT_FALSE(browser_window->IsFullscreen());

  shell_delegate->ToggleFullscreen();
  EXPECT_TRUE(browser_window->IsFullscreen());
  EXPECT_TRUE(IsInImmersiveFullscreen(browser_window));

  shell_delegate->ToggleFullscreen();
  EXPECT_FALSE(browser_window->IsMaximized());
  EXPECT_FALSE(browser_window->IsFullscreen());

  // 2) ToggleFullscreen() should have no effect on windows which cannot be
  // maximized.
  browser_window->GetNativeWindow()->SetProperty(aura::client::kCanMaximizeKey,
                                                 false);
  shell_delegate->ToggleFullscreen();
  EXPECT_FALSE(browser_window->IsMaximized());
  EXPECT_FALSE(browser_window->IsFullscreen());

  // 3) ToggleFullscreen() should maximize v1 app browser windows which use
  // AppNonClientFrameViewAsh.
  // TODO(pkotwicz): Figure out if we actually want this behavior.
  Browser::CreateParams browser_create_params(Browser::TYPE_POPUP,
      browser()->profile(), chrome::HOST_DESKTOP_TYPE_NATIVE);
#if defined(OS_WIN)
  browser_create_params.host_desktop_type = chrome::HOST_DESKTOP_TYPE_ASH;
#endif  // OS_WIN
  browser_create_params.app_name = "Test";
  browser_create_params.app_type = Browser::APP_TYPE_HOST;

  Browser* app_host_browser = new Browser(browser_create_params);
  ASSERT_TRUE(app_host_browser->is_app());
  AddBlankTabAndShow(app_host_browser);
  browser_window = app_host_browser->window();
  ASSERT_TRUE(browser_window->IsActive());
  EXPECT_FALSE(browser_window->IsMaximized());
  EXPECT_FALSE(browser_window->IsFullscreen());

  shell_delegate->ToggleFullscreen();
  EXPECT_TRUE(browser_window->IsMaximized());

  shell_delegate->ToggleFullscreen();
  EXPECT_FALSE(browser_window->IsMaximized());
  EXPECT_FALSE(browser_window->IsFullscreen());

  // 4) ToggleFullscreen() should put child windows of v1 apps into
  // non-immersive fullscreen.
  browser_create_params.host_desktop_type = chrome::HOST_DESKTOP_TYPE_NATIVE;
  browser_create_params.app_type = Browser::APP_TYPE_CHILD;
  Browser* app_child_browser = new Browser(browser_create_params);
  ASSERT_TRUE(app_child_browser->is_app());
  AddBlankTabAndShow(app_child_browser);
  browser_window = app_child_browser->window();
  ASSERT_TRUE(browser_window->IsActive());
  EXPECT_FALSE(browser_window->IsMaximized());
  EXPECT_FALSE(browser_window->IsFullscreen());

  shell_delegate->ToggleFullscreen();
  EXPECT_TRUE(browser_window->IsFullscreen());
  EXPECT_FALSE(IsInImmersiveFullscreen(browser_window));

  shell_delegate->ToggleFullscreen();
  EXPECT_FALSE(browser_window->IsMaximized());
  EXPECT_FALSE(browser_window->IsFullscreen());

  // 5) ToggleFullscreen() should put popup browser windows into non-immersive
  // fullscreen.
  browser_create_params.app_name = "";
  Browser* popup_browser = new Browser(browser_create_params);
  ASSERT_TRUE(popup_browser->is_type_popup());
  ASSERT_FALSE(popup_browser->is_app());
  AddBlankTabAndShow(popup_browser);
  browser_window = popup_browser->window();
  ASSERT_TRUE(browser_window->IsActive());
  EXPECT_FALSE(browser_window->IsMaximized());
  EXPECT_FALSE(browser_window->IsFullscreen());

  shell_delegate->ToggleFullscreen();
  EXPECT_TRUE(browser_window->IsFullscreen());
  EXPECT_FALSE(IsInImmersiveFullscreen(browser_window));

  shell_delegate->ToggleFullscreen();
  EXPECT_FALSE(browser_window->IsMaximized());
  EXPECT_FALSE(browser_window->IsFullscreen());
}

typedef extensions::PlatformAppBrowserTest
    ChromeShellDelegatePlatformAppBrowserTest;

// Test that ToggleFullscreen() toggles the platform app's fullscreen state.
IN_PROC_BROWSER_TEST_F(ChromeShellDelegatePlatformAppBrowserTest,
                       ToggleFullscreenPlatformApp) {
  ash::ShellDelegate* shell_delegate = ash::Shell::GetInstance()->delegate();
  ASSERT_TRUE(shell_delegate);

  const extensions::Extension* extension = LoadAndLaunchPlatformApp("minimal");
  apps::ShellWindow* shell_window = CreateShellWindow(extension);
  apps::NativeAppWindow* app_window = shell_window->GetBaseWindow();
  ASSERT_TRUE(shell_window->GetBaseWindow()->IsActive());
  EXPECT_FALSE(app_window->IsMaximized());
  EXPECT_FALSE(app_window->IsFullscreen());

  shell_delegate->ToggleFullscreen();
  EXPECT_TRUE(app_window->IsFullscreen());

  shell_delegate->ToggleFullscreen();
  EXPECT_FALSE(app_window->IsMaximized());
  EXPECT_FALSE(app_window->IsFullscreen());

  CloseShellWindow(shell_window);
}
