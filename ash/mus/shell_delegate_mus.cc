// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/shell_delegate_mus.h"

#include "ash/default_accessibility_delegate.h"
#include "ash/default_user_wallpaper_delegate.h"
#include "ash/mus/shelf_delegate_mus.h"
#include "ash/session/session_state_delegate.h"
#include "ash/system/tray/default_system_tray_delegate.h"
#include "base/strings/string16.h"
#include "components/user_manager/user_info_impl.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace sysui {

namespace {

class SessionStateDelegateStub : public SessionStateDelegate {
 public:
  SessionStateDelegateStub()
      : screen_locked_(false), user_info_(new user_manager::UserInfoImpl()) {}

  ~SessionStateDelegateStub() override {}

  // SessionStateDelegate:
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
    screen_locked_ = true;
    NOTIMPLEMENTED();
  }
  void UnlockScreen() override {
    NOTIMPLEMENTED();
    screen_locked_ = false;
  }
  bool IsUserSessionBlocked() const override { return false; }
  SessionState GetSessionState() const override { return SESSION_STATE_ACTIVE; }
  const user_manager::UserInfo* GetUserInfo(UserIndex index) const override {
    return user_info_.get();
  }
  bool ShouldShowAvatar(aura::Window* window) const override {
    return !user_info_->GetImage().isNull();
  }
  gfx::ImageSkia GetAvatarImageForWindow(aura::Window* window) const override {
    return gfx::ImageSkia();
  }
  void SwitchActiveUser(const AccountId& account_id) override {}
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

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateStub);
};

}  // namespace

ShellDelegateMus::ShellDelegateMus() {}
ShellDelegateMus::~ShellDelegateMus() {}

bool ShellDelegateMus::IsFirstRunAfterBoot() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellDelegateMus::IsIncognitoAllowed() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellDelegateMus::IsMultiProfilesEnabled() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellDelegateMus::IsRunningInForcedAppMode() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellDelegateMus::CanShowWindowForUser(aura::Window* window) const {
  NOTIMPLEMENTED();
  return true;
}

bool ShellDelegateMus::IsForceMaximizeOnFirstRun() const {
  NOTIMPLEMENTED();
  return false;
}

void ShellDelegateMus::PreInit() {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::PreShutdown() {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::Exit() {
  NOTIMPLEMENTED();
}

keyboard::KeyboardUI* ShellDelegateMus::CreateKeyboardUI() {
  NOTIMPLEMENTED();
  return nullptr;
}

void ShellDelegateMus::VirtualKeyboardActivated(bool activated) {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::AddVirtualKeyboardStateObserver(
    VirtualKeyboardStateObserver* observer) {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::RemoveVirtualKeyboardStateObserver(
    VirtualKeyboardStateObserver* observer) {
  NOTIMPLEMENTED();
}

app_list::AppListViewDelegate* ShellDelegateMus::GetAppListViewDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

ShelfDelegate* ShellDelegateMus::CreateShelfDelegate(ShelfModel* model) {
  return new ShelfDelegateMus(model);
}

ash::SystemTrayDelegate* ShellDelegateMus::CreateSystemTrayDelegate() {
  NOTIMPLEMENTED() << " Using the default SystemTrayDelegate implementation";
  return new DefaultSystemTrayDelegate;
}

ash::UserWallpaperDelegate* ShellDelegateMus::CreateUserWallpaperDelegate() {
  NOTIMPLEMENTED() << " Using the default UserWallpaperDelegate implementation";
  return new DefaultUserWallpaperDelegate();
}

ash::SessionStateDelegate* ShellDelegateMus::CreateSessionStateDelegate() {
  NOTIMPLEMENTED() << " Using a stub SessionStateDeleagte implementation";
  return new SessionStateDelegateStub;
}

ash::AccessibilityDelegate* ShellDelegateMus::CreateAccessibilityDelegate() {
  NOTIMPLEMENTED() << " Using the default AccessibilityDelegate implementation";
  return new DefaultAccessibilityDelegate;
}

ash::NewWindowDelegate* ShellDelegateMus::CreateNewWindowDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

ash::MediaDelegate* ShellDelegateMus::CreateMediaDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

ui::MenuModel* ShellDelegateMus::CreateContextMenu(
    aura::Window* root_window,
    ash::ShelfItemDelegate* item_delegate,
    ash::ShelfItem* item) {
  NOTIMPLEMENTED();
  return nullptr;
}

GPUSupport* ShellDelegateMus::CreateGPUSupport() {
  NOTIMPLEMENTED();
  return nullptr;
}

base::string16 ShellDelegateMus::GetProductName() const {
  NOTIMPLEMENTED();
  return base::string16();
}

gfx::Image ShellDelegateMus::GetDeprecatedAcceleratorImage() const {
  NOTIMPLEMENTED();
  return gfx::Image();
}

}  // namespace sysui
}  // namespace ash
