// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/shell_delegate_mus.h"

#include <utility>

#include "ash/common/gpu_support_stub.h"
#include "ash/common/palette_delegate.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/mus/accessibility_delegate_mus.h"
#include "ash/mus/context_menu_mus.h"
#include "ash/mus/shelf_delegate_mus.h"
#include "ash/mus/system_tray_delegate_mus.h"
#include "ash/mus/wallpaper_delegate_mus.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "components/user_manager/user_info_impl.h"
#include "ui/gfx/image/image.h"

namespace ash {
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
  bool ShouldLockScreenAutomatically() const override { return false; }
  void LockScreen() override {
    screen_locked_ = true;
    NOTIMPLEMENTED();
  }
  void UnlockScreen() override {
    NOTIMPLEMENTED();
    screen_locked_ = false;
  }
  bool IsUserSessionBlocked() const override { return false; }
  session_manager::SessionState GetSessionState() const override {
    return session_manager::SessionState::ACTIVE;
  }
  const user_manager::UserInfo* GetUserInfo(UserIndex index) const override {
    return user_info_.get();
  }
  bool ShouldShowAvatar(WmWindow* window) const override {
    NOTIMPLEMENTED();
    return !user_info_->GetImage().isNull();
  }
  gfx::ImageSkia GetAvatarImageForWindow(WmWindow* window) const override {
    NOTIMPLEMENTED();
    return gfx::ImageSkia();
  }
  void SwitchActiveUser(const AccountId& account_id) override {}
  void CycleActiveUser(CycleUserDirection direction) override {}
  bool IsMultiProfileAllowedByPrimaryUserPolicy() const override {
    return true;
  }
  void AddSessionStateObserver(ash::SessionStateObserver* observer) override {}
  void RemoveSessionStateObserver(
      ash::SessionStateObserver* observer) override {}

 private:
  bool screen_locked_;

  // A pseudo user info.
  std::unique_ptr<user_manager::UserInfo> user_info_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateStub);
};

}  // namespace

ShellDelegateMus::ShellDelegateMus(service_manager::Connector* connector)
    : connector_(connector) {}

ShellDelegateMus::~ShellDelegateMus() {}

service_manager::Connector* ShellDelegateMus::GetShellConnector() const {
  return connector_;
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

bool ShellDelegateMus::CanShowWindowForUser(WmWindow* window) const {
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

void ShellDelegateMus::OpenUrlFromArc(const GURL& url) {
  NOTIMPLEMENTED();
}

ShelfDelegate* ShellDelegateMus::CreateShelfDelegate(ShelfModel* model) {
  return new ShelfDelegateMus();
}

SystemTrayDelegate* ShellDelegateMus::CreateSystemTrayDelegate() {
  return new SystemTrayDelegateMus();
}

std::unique_ptr<WallpaperDelegate> ShellDelegateMus::CreateWallpaperDelegate() {
  return base::MakeUnique<WallpaperDelegateMus>();
}

SessionStateDelegate* ShellDelegateMus::CreateSessionStateDelegate() {
  // TODO: http://crbug.com/647416.
  NOTIMPLEMENTED() << " Using a stub SessionStateDeleagte implementation";
  return new SessionStateDelegateStub;
}

AccessibilityDelegate* ShellDelegateMus::CreateAccessibilityDelegate() {
  return new AccessibilityDelegateMus(connector_);
}

std::unique_ptr<PaletteDelegate> ShellDelegateMus::CreatePaletteDelegate() {
  // TODO: http://crbug.com/647417.
  NOTIMPLEMENTED();
  return nullptr;
}

ui::MenuModel* ShellDelegateMus::CreateContextMenu(WmShelf* wm_shelf,
                                                   const ShelfItem* item) {
  return new ContextMenuMus(wm_shelf);
}

GPUSupport* ShellDelegateMus::CreateGPUSupport() {
  // TODO: http://crbug.com/647421.
  NOTIMPLEMENTED() << " Using a stub GPUSupport implementation";
  return new GPUSupportStub();
}

base::string16 ShellDelegateMus::GetProductName() const {
  NOTIMPLEMENTED();
  return base::string16();
}

gfx::Image ShellDelegateMus::GetDeprecatedAcceleratorImage() const {
  NOTIMPLEMENTED();
  return gfx::Image();
}

bool ShellDelegateMus::IsTouchscreenEnabledInPrefs(bool use_local_state) const {
  NOTIMPLEMENTED();
  return true;
}

void ShellDelegateMus::SetTouchscreenEnabledInPrefs(bool enabled,
                                                    bool use_local_state) {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::UpdateTouchscreenStatusFromPrefs() {
  NOTIMPLEMENTED();
}

}  // namespace ash
