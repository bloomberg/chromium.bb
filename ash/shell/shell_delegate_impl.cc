// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shell_delegate_impl.h"

#include "ash/shell/example_factory.h"
#include "ash/shell/launcher_delegate_impl.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/partial_screenshot_view.h"
#include "base/message_loop.h"
#include "ui/aura/window.h"

namespace ash {
namespace shell {

ShellDelegateImpl::ShellDelegateImpl()
    : watcher_(NULL),
      launcher_delegate_(NULL),
      locked_(false) {
}

ShellDelegateImpl::~ShellDelegateImpl() {
}

void ShellDelegateImpl::SetWatcher(WindowWatcher* watcher) {
  watcher_ = watcher;
  if (launcher_delegate_)
    launcher_delegate_->set_watcher(watcher);
}

views::Widget* ShellDelegateImpl::CreateStatusArea() {
  return NULL;
}

bool ShellDelegateImpl::CanCreateLauncher() {
  return true;
}

void ShellDelegateImpl::LockScreen() {
  ash::shell::CreateLockScreen();
  locked_ = true;
}

void ShellDelegateImpl::UnlockScreen() {
  locked_ = false;
}

bool ShellDelegateImpl::IsScreenLocked() const {
  return locked_;
}

void ShellDelegateImpl::Exit() {
  MessageLoopForUI::current()->Quit();
}

ash::AppListViewDelegate* ShellDelegateImpl::CreateAppListViewDelegate() {
  return ash::shell::CreateAppListViewDelegate();
}

std::vector<aura::Window*> ShellDelegateImpl::GetCycleWindowList(
    CycleSource source) const {
  aura::Window* default_container = ash::Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_DefaultContainer);
  std::vector<aura::Window*> windows = default_container->children();
  // Window cycling expects the topmost window at the front of the list.
  std::reverse(windows.begin(), windows.end());
  return windows;
}

void ShellDelegateImpl::StartPartialScreenshot(
    ash::ScreenshotDelegate* screenshot_delegate) {
  ash::PartialScreenshotView::StartPartialScreenshot(screenshot_delegate);
}

ash::LauncherDelegate* ShellDelegateImpl::CreateLauncherDelegate(
    ash::LauncherModel* model) {
  launcher_delegate_ = new LauncherDelegateImpl(watcher_);
  return launcher_delegate_;
}

ash::SystemTrayDelegate* ShellDelegateImpl::CreateSystemTrayDelegate(
    ash::SystemTray* tray) {
  return NULL;
}

ash::UserWallpaperDelegate* ShellDelegateImpl::CreateUserWallpaperDelegate() {
  return NULL;
}

}  // namespace shell
}  // namespace ash
