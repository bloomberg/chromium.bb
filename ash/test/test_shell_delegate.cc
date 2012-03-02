// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shell_delegate.h"

#include <algorithm>

#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

TestShellDelegate::TestShellDelegate()
    : override_window_mode_(false),
      window_mode_(Shell::MODE_OVERLAPPING) {
}

TestShellDelegate::~TestShellDelegate() {
}

void TestShellDelegate::SetOverrideWindowMode(Shell::WindowMode window_mode) {
  override_window_mode_ = true;
  window_mode_ = window_mode;
}

views::Widget* TestShellDelegate::CreateStatusArea() {
  return NULL;
}

#if defined(OS_CHROMEOS)
void TestShellDelegate::LockScreen() {
}
#endif

void TestShellDelegate::Exit() {
}

AppListViewDelegate* TestShellDelegate::CreateAppListViewDelegate() {
  return NULL;
}

std::vector<aura::Window*> TestShellDelegate::GetCycleWindowList(
    CycleSource source,
    CycleOrder order) const {
  // We just use the Shell's default container of windows, so tests can be
  // written with the usual CreateTestWindowWithId() calls. But window cycling
  // expects the topmost window at the front of the list, so reverse the order
  // if we are mimicking MRU.
  aura::Window* default_container = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_DefaultContainer);
  std::vector<aura::Window*> windows = default_container->children();
  if (order != ShellDelegate::ORDER_LINEAR)
    std::reverse(windows.begin(), windows.end());
  return windows;
}

void TestShellDelegate::StartPartialScreenshot(
    ScreenshotDelegate* screenshot_delegate) {
  if (screenshot_delegate)
    screenshot_delegate->HandleTakePartialScreenshot(NULL, gfx::Rect());
}

LauncherDelegate* TestShellDelegate::CreateLauncherDelegate(
    ash::LauncherModel* model) {
  return NULL;
}

SystemTrayDelegate* TestShellDelegate::CreateSystemTrayDelegate(
    SystemTray* tray) {
  return NULL;
}

bool TestShellDelegate::GetOverrideWindowMode(Shell::WindowMode* window_mode) {
  if (override_window_mode_) {
    *window_mode = window_mode_;
    return true;
  }
  return false;
}

}  // namespace test
}  // namespace ash
