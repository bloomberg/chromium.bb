// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shell_delegate_impl.h"

#include "ash/shell/example_factory.h"
#include "ash/shell/launcher_delegate_impl.h"
#include "ash/shell/toplevel_window.h"
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

bool ShellDelegateImpl::IsUserLoggedIn() {
  return true;
}

void ShellDelegateImpl::LockScreen() {
  ash::shell::CreateLockScreen();
  locked_ = true;
  ash::Shell::GetInstance()->UpdateShelfVisibility();
}

void ShellDelegateImpl::UnlockScreen() {
  locked_ = false;
  ash::Shell::GetInstance()->UpdateShelfVisibility();
}

bool ShellDelegateImpl::IsScreenLocked() const {
  return locked_;
}

void ShellDelegateImpl::Shutdown() {
}

void ShellDelegateImpl::Exit() {
  MessageLoopForUI::current()->Quit();
}

void ShellDelegateImpl::NewWindow(bool incognito) {
  ash::shell::ToplevelWindow::CreateParams create_params;
  create_params.can_resize = true;
  create_params.can_maximize = true;
  ash::shell::ToplevelWindow::CreateToplevelWindow(create_params);
}

void ShellDelegateImpl::Search() {
}

void ShellDelegateImpl::OpenFileManager() {
}

void ShellDelegateImpl::OpenCrosh() {
}

void ShellDelegateImpl::OpenMobileSetup() {
}

content::BrowserContext* ShellDelegateImpl::GetCurrentBrowserContext() {
  return Shell::GetInstance()->browser_context();
}

void ShellDelegateImpl::ToggleSpokenFeedback() {
}

app_list::AppListViewDelegate* ShellDelegateImpl::CreateAppListViewDelegate() {
  return ash::shell::CreateAppListViewDelegate();
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
