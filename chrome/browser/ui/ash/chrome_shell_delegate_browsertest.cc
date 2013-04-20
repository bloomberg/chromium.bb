// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/test/base/in_process_browser_test.h"

typedef InProcessBrowserTest ChromeShellDelegateBrowserTest;

// Confirm that toggling window miximized works properly
IN_PROC_BROWSER_TEST_F(ChromeShellDelegateBrowserTest, ToggleMaximized) {
  ash::ShellDelegate* shell_delegate = ash::Shell::GetInstance()->delegate();
  ASSERT_TRUE(shell_delegate);
  aura::Window* window = ash::wm::GetActiveWindow();
  ASSERT_TRUE(window);

  if (chrome::UseImmersiveFullscreen()) {
    // "ToggleMaximized" toggles immersive fullscreen.
    EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
    EXPECT_FALSE(ash::wm::IsWindowFullscreen(window));
    shell_delegate->ToggleMaximized();
    EXPECT_TRUE(ash::wm::IsWindowFullscreen(window));
    shell_delegate->ToggleMaximized();
    EXPECT_FALSE(ash::wm::IsWindowFullscreen(window));
    return;
  }

  // When not in fullscreen, ShellDelegate::ToggleMaximized toggles Maximized.
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
  shell_delegate->ToggleMaximized();
  EXPECT_TRUE(ash::wm::IsWindowMaximized(window));
  shell_delegate->ToggleMaximized();
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));

  // When in fullscreen ShellDelegate::ToggleMaximized gets out of fullscreen.
  EXPECT_FALSE(ash::wm::IsWindowFullscreen(window));
  Browser* browser = chrome::FindBrowserWithWindow(window);
  ASSERT_TRUE(browser);
  chrome::ToggleFullscreenMode(browser);
  EXPECT_TRUE(ash::wm::IsWindowFullscreen(window));
  shell_delegate->ToggleMaximized();
  EXPECT_FALSE(ash::wm::IsWindowFullscreen(window));
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
  shell_delegate->ToggleMaximized();
  EXPECT_FALSE(ash::wm::IsWindowFullscreen(window));
  EXPECT_TRUE(ash::wm::IsWindowMaximized(window));
}
