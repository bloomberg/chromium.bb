// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shell_delegate_impl.h"

#include "ash/accessibility_delegate.h"
#include "ash/caps_lock_delegate_stub.h"
#include "ash/default_accessibility_delegate.h"
#include "ash/default_user_wallpaper_delegate.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/keyboard_controller_proxy_stub.h"
#include "ash/session_state_delegate.h"
#include "ash/session_state_delegate_stub.h"
#include "ash/shell/context_menu.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/launcher_delegate_impl.h"
#include "ash/shell/toplevel_window.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/default_system_tray_delegate.h"
#include "ash/wm/window_state.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/window.h"
#include "ui/views/corewm/input_method_event_filter.h"

namespace ash {
namespace shell {

ShellDelegateImpl::ShellDelegateImpl()
    : watcher_(NULL),
      launcher_delegate_(NULL) {
}

ShellDelegateImpl::~ShellDelegateImpl() {
}

void ShellDelegateImpl::SetWatcher(WindowWatcher* watcher) {
  watcher_ = watcher;
  if (launcher_delegate_)
    launcher_delegate_->set_watcher(watcher);
}

bool ShellDelegateImpl::IsFirstRunAfterBoot() const {
  return false;
}

bool ShellDelegateImpl::IsMultiProfilesEnabled() const {
  return false;
}

bool ShellDelegateImpl::IsRunningInForcedAppMode() const {
  return false;
}

void ShellDelegateImpl::PreInit() {
}

void ShellDelegateImpl::Shutdown() {
}

void ShellDelegateImpl::Exit() {
  base::MessageLoopForUI::current()->Quit();
}

void ShellDelegateImpl::NewTab() {
}

void ShellDelegateImpl::NewWindow(bool incognito) {
  ash::shell::ToplevelWindow::CreateParams create_params;
  create_params.can_resize = true;
  create_params.can_maximize = true;
  ash::shell::ToplevelWindow::CreateToplevelWindow(create_params);
}

void ShellDelegateImpl::ToggleFullscreen() {
  // TODO(oshima): Remove this when crbug.com/309837 is implemented.
  wm::WindowState* window_state = wm::GetActiveWindowState();
  if (window_state)
    window_state->ToggleMaximized();
}

void ShellDelegateImpl::OpenFileManager() {
}

void ShellDelegateImpl::OpenCrosh() {
}

void ShellDelegateImpl::RestoreTab() {
}

void ShellDelegateImpl::ShowKeyboardOverlay() {
}

keyboard::KeyboardControllerProxy*
    ShellDelegateImpl::CreateKeyboardControllerProxy() {
  return new KeyboardControllerProxyStub();
}

void ShellDelegateImpl::ShowTaskManager() {
}

content::BrowserContext* ShellDelegateImpl::GetCurrentBrowserContext() {
  return Shell::GetInstance()->browser_context();
}

app_list::AppListViewDelegate* ShellDelegateImpl::CreateAppListViewDelegate() {
  return ash::shell::CreateAppListViewDelegate();
}

ash::LauncherDelegate* ShellDelegateImpl::CreateLauncherDelegate(
    ash::LauncherModel* model) {
  launcher_delegate_ = new LauncherDelegateImpl(watcher_);
  return launcher_delegate_;
}

ash::SystemTrayDelegate* ShellDelegateImpl::CreateSystemTrayDelegate() {
  return new DefaultSystemTrayDelegate;
}

ash::UserWallpaperDelegate* ShellDelegateImpl::CreateUserWallpaperDelegate() {
  return new DefaultUserWallpaperDelegate();
}

ash::CapsLockDelegate* ShellDelegateImpl::CreateCapsLockDelegate() {
  return new CapsLockDelegateStub;
}

ash::SessionStateDelegate* ShellDelegateImpl::CreateSessionStateDelegate() {
  return new SessionStateDelegateStub;
}

ash::AccessibilityDelegate* ShellDelegateImpl::CreateAccessibilityDelegate() {
  return new internal::DefaultAccessibilityDelegate;
}

aura::client::UserActionClient* ShellDelegateImpl::CreateUserActionClient() {
  return NULL;
}

void ShellDelegateImpl::OpenFeedbackPage() {
}

void ShellDelegateImpl::RecordUserMetricsAction(UserMetricsAction action) {
}

void ShellDelegateImpl::HandleMediaNextTrack() {
}

void ShellDelegateImpl::HandleMediaPlayPause() {
}

void ShellDelegateImpl::HandleMediaPrevTrack() {
}

ui::MenuModel* ShellDelegateImpl::CreateContextMenu(aura::RootWindow* root) {
  return new ContextMenu(root);
}

RootWindowHostFactory* ShellDelegateImpl::CreateRootWindowHostFactory() {
  return RootWindowHostFactory::Create();
}

base::string16 ShellDelegateImpl::GetProductName() const {
  return base::string16();
}

}  // namespace shell
}  // namespace ash
