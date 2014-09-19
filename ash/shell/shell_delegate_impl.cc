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
  virtual ~NewWindowDelegateImpl() {}

  // NewWindowDelegate:
  virtual void NewTab() OVERRIDE {}
  virtual void NewWindow(bool incognito) OVERRIDE {
    ash::shell::ToplevelWindow::CreateParams create_params;
    create_params.can_resize = true;
    create_params.can_maximize = true;
    ash::shell::ToplevelWindow::CreateToplevelWindow(create_params);
  }
  virtual void OpenFileManager() OVERRIDE {}
  virtual void OpenCrosh() OVERRIDE {}
  virtual void RestoreTab() OVERRIDE {}
  virtual void ShowKeyboardOverlay() OVERRIDE {}
  virtual void ShowTaskManager() OVERRIDE {}
  virtual void OpenFeedbackPage() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWindowDelegateImpl);
};

class MediaDelegateImpl : public MediaDelegate {
 public:
  MediaDelegateImpl() {}
  virtual ~MediaDelegateImpl() {}

  // MediaDelegate:
  virtual void HandleMediaNextTrack() OVERRIDE {}
  virtual void HandleMediaPlayPause() OVERRIDE {}
  virtual void HandleMediaPrevTrack() OVERRIDE {}
  virtual MediaCaptureState GetMediaCaptureState(
      content::BrowserContext* context) OVERRIDE {
    return MEDIA_CAPTURE_VIDEO;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaDelegateImpl);
};

class SessionStateDelegateImpl : public SessionStateDelegate {
 public:
  SessionStateDelegateImpl()
      : screen_locked_(false), user_info_(new user_manager::UserInfoImpl()) {}

  virtual ~SessionStateDelegateImpl() {}

  // SessionStateDelegate:
  virtual content::BrowserContext* GetBrowserContextByIndex(
      MultiProfileIndex index) OVERRIDE {
    return Shell::GetInstance()->delegate()->GetActiveBrowserContext();
  }
  virtual content::BrowserContext* GetBrowserContextForWindow(
      aura::Window* window) OVERRIDE {
    return Shell::GetInstance()->delegate()->GetActiveBrowserContext();
  }
  virtual int GetMaximumNumberOfLoggedInUsers() const OVERRIDE { return 3; }
  virtual int NumberOfLoggedInUsers() const OVERRIDE {
    // ash_shell has 2 users.
    return 2;
  }
  virtual bool IsActiveUserSessionStarted() const OVERRIDE { return true; }
  virtual bool CanLockScreen() const OVERRIDE { return true; }
  virtual bool IsScreenLocked() const OVERRIDE { return screen_locked_; }
  virtual bool ShouldLockScreenBeforeSuspending() const OVERRIDE {
    return false;
  }
  virtual void LockScreen() OVERRIDE {
    shell::CreateLockScreen();
    screen_locked_ = true;
    Shell::GetInstance()->UpdateShelfVisibility();
  }
  virtual void UnlockScreen() OVERRIDE {
    screen_locked_ = false;
    Shell::GetInstance()->UpdateShelfVisibility();
  }
  virtual bool IsUserSessionBlocked() const OVERRIDE {
    return !IsActiveUserSessionStarted() || IsScreenLocked();
  }
  virtual SessionState GetSessionState() const OVERRIDE {
    // Assume that if session is not active we're at login.
    return IsActiveUserSessionStarted() ? SESSION_STATE_ACTIVE
                                        : SESSION_STATE_LOGIN_PRIMARY;
  }
  virtual const user_manager::UserInfo* GetUserInfo(
      MultiProfileIndex index) const OVERRIDE {
    return user_info_.get();
  }
  virtual const user_manager::UserInfo* GetUserInfo(
      content::BrowserContext* context) const OVERRIDE {
    return user_info_.get();
  }
  virtual bool ShouldShowAvatar(aura::Window* window) const OVERRIDE {
    return !user_info_->GetImage().isNull();
  }
  virtual void SwitchActiveUser(const std::string& user_id) OVERRIDE {}
  virtual void CycleActiveUser(CycleUser cycle_user) OVERRIDE {}
  virtual bool IsMultiProfileAllowedByPrimaryUserPolicy() const OVERRIDE {
    return true;
  }
  virtual void AddSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE {}
  virtual void RemoveSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE {}

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
