// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shell_delegate_impl.h"

#include "ash/accessibility_delegate.h"
#include "ash/default_accessibility_delegate.h"
#include "ash/default_user_wallpaper_delegate.h"
#include "ash/gpu_support_stub.h"
#include "ash/media_delegate.h"
#include "ash/new_window_delegate.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell/context_menu.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/keyboard_controller_proxy_stub.h"
#include "ash/shell/shelf_delegate_impl.h"
#include "ash/shell/toplevel_window.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/default_system_tray_delegate.h"
#include "ash/wm/window_state.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user_info_impl.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/aura/window.h"
#include "ui/wm/core/input_method_event_filter.h"

namespace ash {
namespace shell {
namespace {

class NewWindowDelegateImpl : public NewWindowDelegate {
 public:
  NewWindowDelegateImpl() {}
  ~NewWindowDelegateImpl() override {}

  // NewWindowDelegate:
  void NewTab() override {}
  void NewWindow(bool incognito) override {
    ash::shell::ToplevelWindow::CreateParams create_params;
    create_params.can_resize = true;
    create_params.can_maximize = true;
    ash::shell::ToplevelWindow::CreateToplevelWindow(create_params);
  }
  void OpenFileManager() override {}
  void OpenCrosh() override {}
  void OpenGetHelp() override {}
  void RestoreTab() override {}
  void ShowKeyboardOverlay() override {}
  void ShowTaskManager() override {}
  void OpenFeedbackPage() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWindowDelegateImpl);
};

class MediaDelegateImpl : public MediaDelegate {
 public:
  MediaDelegateImpl() {}
  ~MediaDelegateImpl() override {}

  // MediaDelegate:
  void HandleMediaNextTrack() override {}
  void HandleMediaPlayPause() override {}
  void HandleMediaPrevTrack() override {}
  MediaCaptureState GetMediaCaptureState(
      content::BrowserContext* context) override {
    return MEDIA_CAPTURE_VIDEO;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaDelegateImpl);
};

class SessionStateDelegateImpl : public SessionStateDelegate {
 public:
  SessionStateDelegateImpl()
      : screen_locked_(false), user_info_(new user_manager::UserInfoImpl()) {}

  ~SessionStateDelegateImpl() override {}

  // SessionStateDelegate:
  content::BrowserContext* GetBrowserContextByIndex(
      MultiProfileIndex index) override {
    return Shell::GetInstance()->delegate()->GetActiveBrowserContext();
  }
  content::BrowserContext* GetBrowserContextForWindow(
      aura::Window* window) override {
    return Shell::GetInstance()->delegate()->GetActiveBrowserContext();
  }
  int GetMaximumNumberOfLoggedInUsers() const override { return 3; }
  int NumberOfLoggedInUsers() const override {
    // ash_shell has 2 users.
    return 2;
  }
  bool IsActiveUserSessionStarted() const override { return true; }
  bool CanLockScreen() const override { return true; }
  bool IsScreenLocked() const override { return screen_locked_; }
  bool ShouldLockScreenBeforeSuspending() const override { return false; }
  void LockScreen() override {
    shell::CreateLockScreen();
    screen_locked_ = true;
    Shell::GetInstance()->UpdateShelfVisibility();
  }
  void UnlockScreen() override {
    screen_locked_ = false;
    Shell::GetInstance()->UpdateShelfVisibility();
  }
  bool IsUserSessionBlocked() const override {
    return !IsActiveUserSessionStarted() || IsScreenLocked();
  }
  SessionState GetSessionState() const override {
    // Assume that if session is not active we're at login.
    return IsActiveUserSessionStarted() ? SESSION_STATE_ACTIVE
                                        : SESSION_STATE_LOGIN_PRIMARY;
  }
  const user_manager::UserInfo* GetUserInfo(
      MultiProfileIndex index) const override {
    return user_info_.get();
  }
  const user_manager::UserInfo* GetUserInfo(
      content::BrowserContext* context) const override {
    return user_info_.get();
  }
  bool ShouldShowAvatar(aura::Window* window) const override {
    return !user_info_->GetImage().isNull();
  }
  void SwitchActiveUser(const std::string& user_id) override {}
  void CycleActiveUser(CycleUser cycle_user) override {}
  bool IsMultiProfileAllowedByPrimaryUserPolicy() const override {
    return true;
  }
  void AddSessionStateObserver(ash::SessionStateObserver* observer) override {}
  void RemoveSessionStateObserver(
      ash::SessionStateObserver* observer) override {}

 private:
  bool screen_locked_;

  // A pseudo user info.
  scoped_ptr<user_manager::UserInfo> user_info_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateImpl);
};

}  // namespace

ShellDelegateImpl::ShellDelegateImpl()
    : watcher_(NULL),
      shelf_delegate_(NULL),
      browser_context_(NULL) {
}

ShellDelegateImpl::~ShellDelegateImpl() {
}

void ShellDelegateImpl::SetWatcher(WindowWatcher* watcher) {
  watcher_ = watcher;
  if (shelf_delegate_)
    shelf_delegate_->set_watcher(watcher);
}

bool ShellDelegateImpl::IsFirstRunAfterBoot() const {
  return false;
}

bool ShellDelegateImpl::IsIncognitoAllowed() const {
  return true;
}

bool ShellDelegateImpl::IsMultiProfilesEnabled() const {
  return false;
}

bool ShellDelegateImpl::IsRunningInForcedAppMode() const {
  return false;
}

bool ShellDelegateImpl::IsMultiAccountEnabled() const {
  return false;
}

void ShellDelegateImpl::PreInit() {
}

void ShellDelegateImpl::PreShutdown() {
}

void ShellDelegateImpl::Exit() {
  base::MessageLoopForUI::current()->Quit();
}

keyboard::KeyboardControllerProxy*
    ShellDelegateImpl::CreateKeyboardControllerProxy() {
  return new KeyboardControllerProxyStub();
}

void ShellDelegateImpl::VirtualKeyboardActivated(bool activated) {
}

void ShellDelegateImpl::AddVirtualKeyboardStateObserver(
    VirtualKeyboardStateObserver* observer) {
}

void ShellDelegateImpl::RemoveVirtualKeyboardStateObserver(
    VirtualKeyboardStateObserver* observer) {
}

content::BrowserContext* ShellDelegateImpl::GetActiveBrowserContext() {
  return browser_context_;
}

app_list::AppListViewDelegate* ShellDelegateImpl::GetAppListViewDelegate() {
  if (!app_list_view_delegate_)
    app_list_view_delegate_.reset(ash::shell::CreateAppListViewDelegate());
  return app_list_view_delegate_.get();
}

ShelfDelegate* ShellDelegateImpl::CreateShelfDelegate(ShelfModel* model) {
  shelf_delegate_ = new ShelfDelegateImpl(watcher_);
  return shelf_delegate_;
}

ash::SystemTrayDelegate* ShellDelegateImpl::CreateSystemTrayDelegate() {
  return new DefaultSystemTrayDelegate;
}

ash::UserWallpaperDelegate* ShellDelegateImpl::CreateUserWallpaperDelegate() {
  return new DefaultUserWallpaperDelegate();
}

ash::SessionStateDelegate* ShellDelegateImpl::CreateSessionStateDelegate() {
  return new SessionStateDelegateImpl;
}

ash::AccessibilityDelegate* ShellDelegateImpl::CreateAccessibilityDelegate() {
  return new DefaultAccessibilityDelegate;
}

ash::NewWindowDelegate* ShellDelegateImpl::CreateNewWindowDelegate() {
  return new NewWindowDelegateImpl;
}

ash::MediaDelegate* ShellDelegateImpl::CreateMediaDelegate() {
  return new MediaDelegateImpl;
}

ui::MenuModel* ShellDelegateImpl::CreateContextMenu(
    aura::Window* root,
    ash::ShelfItemDelegate* item_delegate,
    ash::ShelfItem* item) {
  return new ContextMenu(root);
}

GPUSupport* ShellDelegateImpl::CreateGPUSupport() {
  // Real GPU support depends on src/content, so just use a stub.
  return new GPUSupportStub;
}

base::string16 ShellDelegateImpl::GetProductName() const {
  return base::string16();
}

}  // namespace shell
}  // namespace ash
