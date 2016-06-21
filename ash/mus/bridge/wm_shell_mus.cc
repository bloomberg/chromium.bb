// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_shell_mus.h"

#include "ash/common/default_accessibility_delegate.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_observer.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/tray/default_system_tray_delegate.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/window_resizer.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/common/wm_shell_common.h"
#include "ash/mus/bridge/wm_root_window_controller_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/container_ids.h"
#include "ash/mus/drag_window_resizer.h"
#include "ash/mus/root_window_controller.h"
#include "base/memory/ptr_util.h"
#include "components/mus/common/util.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "components/user_manager/user_info_impl.h"

namespace ash {
namespace mus {

namespace {

// TODO(jamescook): After ShellDelegate is ported to ash/common use
// ShellDelegate::CreateSessionStateDelegate() to construct the mus version
// of SessionStateDelegate.
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
  bool ShouldShowAvatar(WmWindow* window) const override {
    NOTIMPLEMENTED();
    return !user_info_->GetImage().isNull();
  }
  gfx::ImageSkia GetAvatarImageForWindow(WmWindow* window) const override {
    NOTIMPLEMENTED();
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
  std::unique_ptr<user_manager::UserInfo> user_info_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateStub);
};

}  // namespace

WmShellMus::WmShellMus(::mus::WindowTreeClient* client)
    : client_(client), session_state_delegate_(new SessionStateDelegateStub) {
  client_->AddObserver(this);
  WmShell::Set(this);

  wm_shell_common_.reset(new WmShellCommon);
  wm_shell_common_->CreateMruWindowTracker();

  accessibility_delegate_.reset(new DefaultAccessibilityDelegate);
  SetSystemTrayDelegate(base::WrapUnique(new DefaultSystemTrayDelegate));
}

WmShellMus::~WmShellMus() {
  SetSystemTrayDelegate(nullptr);
  wm_shell_common_->DeleteMruWindowTracker();
  RemoveClientObserver();
  WmShell::Set(nullptr);
}

// static
WmShellMus* WmShellMus::Get() {
  return static_cast<WmShellMus*>(WmShell::Get());
}

void WmShellMus::AddRootWindowController(
    WmRootWindowControllerMus* controller) {
  root_window_controllers_.push_back(controller);
}

void WmShellMus::RemoveRootWindowController(
    WmRootWindowControllerMus* controller) {
  auto iter = std::find(root_window_controllers_.begin(),
                        root_window_controllers_.end(), controller);
  DCHECK(iter != root_window_controllers_.end());
  root_window_controllers_.erase(iter);
}

// static
WmWindowMus* WmShellMus::GetToplevelAncestor(::mus::Window* window) {
  while (window) {
    if (IsActivationParent(window->parent()))
      return WmWindowMus::Get(window);
    window = window->parent();
  }
  return nullptr;
}

WmRootWindowControllerMus* WmShellMus::GetRootWindowControllerWithDisplayId(
    int64_t id) {
  for (WmRootWindowControllerMus* root_window_controller :
       root_window_controllers_) {
    if (root_window_controller->GetDisplay().id() == id)
      return root_window_controller;
  }
  NOTREACHED();
  return nullptr;
}

MruWindowTracker* WmShellMus::GetMruWindowTracker() {
  return wm_shell_common_->mru_window_tracker();
}

WmWindow* WmShellMus::NewContainerWindow() {
  return WmWindowMus::Get(client_->NewWindow());
}

WmWindow* WmShellMus::GetFocusedWindow() {
  return WmWindowMus::Get(client_->GetFocusedWindow());
}

WmWindow* WmShellMus::GetActiveWindow() {
  return GetToplevelAncestor(client_->GetFocusedWindow());
}

WmWindow* WmShellMus::GetPrimaryRootWindow() {
  return root_window_controllers_[0]->GetWindow();
}

WmWindow* WmShellMus::GetRootWindowForDisplayId(int64_t display_id) {
  return GetRootWindowControllerWithDisplayId(display_id)->GetWindow();
}

WmWindow* WmShellMus::GetRootWindowForNewWindows() {
  NOTIMPLEMENTED();
  return root_window_controllers_[0]->GetWindow();
}

bool WmShellMus::IsForceMaximizeOnFirstRun() {
  NOTIMPLEMENTED();
  return false;
}

bool WmShellMus::IsPinned() {
  NOTIMPLEMENTED();
  return false;
}

void WmShellMus::SetPinnedWindow(WmWindow* window) {
  NOTIMPLEMENTED();
}

bool WmShellMus::CanShowWindowForUser(WmWindow* window) {
  NOTIMPLEMENTED();
  return true;
}

void WmShellMus::LockCursor() {
  NOTIMPLEMENTED();
}

void WmShellMus::UnlockCursor() {
  NOTIMPLEMENTED();
}

std::vector<WmWindow*> WmShellMus::GetAllRootWindows() {
  std::vector<WmWindow*> wm_windows(root_window_controllers_.size());
  for (size_t i = 0; i < root_window_controllers_.size(); ++i)
    wm_windows[i] = root_window_controllers_[i]->GetWindow();
  return wm_windows;
}

void WmShellMus::RecordUserMetricsAction(UserMetricsAction action) {
  NOTIMPLEMENTED();
}

std::unique_ptr<WindowResizer> WmShellMus::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  return base::WrapUnique(
      new DragWindowResizer(std::move(next_window_resizer), window_state));
}

void WmShellMus::OnOverviewModeStarting() {
  FOR_EACH_OBSERVER(ShellObserver, *wm_shell_common_->shell_observers(),
                    OnOverviewModeStarting());
}

void WmShellMus::OnOverviewModeEnded() {
  FOR_EACH_OBSERVER(ShellObserver, *wm_shell_common_->shell_observers(),
                    OnOverviewModeEnded());
}

bool WmShellMus::IsOverviewModeSelecting() {
  NOTIMPLEMENTED();
  return false;
}

bool WmShellMus::IsOverviewModeRestoringMinimizedWindows() {
  NOTIMPLEMENTED();
  return false;
}

AccessibilityDelegate* WmShellMus::GetAccessibilityDelegate() {
  return accessibility_delegate_.get();
}

SessionStateDelegate* WmShellMus::GetSessionStateDelegate() {
  return session_state_delegate_.get();
}

void WmShellMus::AddActivationObserver(WmActivationObserver* observer) {
  activation_observers_.AddObserver(observer);
}

void WmShellMus::RemoveActivationObserver(WmActivationObserver* observer) {
  activation_observers_.RemoveObserver(observer);
}

void WmShellMus::AddDisplayObserver(WmDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

void WmShellMus::RemoveDisplayObserver(WmDisplayObserver* observer) {
  NOTIMPLEMENTED();
}

void WmShellMus::AddShellObserver(ShellObserver* observer) {
  wm_shell_common_->AddShellObserver(observer);
}

void WmShellMus::RemoveShellObserver(ShellObserver* observer) {
  wm_shell_common_->RemoveShellObserver(observer);
}

// static
bool WmShellMus::IsActivationParent(::mus::Window* window) {
  return window && IsActivatableShellWindowId(
                       WmWindowMus::Get(window)->GetShellWindowId());
}

void WmShellMus::RemoveClientObserver() {
  if (!client_)
    return;

  client_->RemoveObserver(this);
  client_ = nullptr;
}

// TODO: support OnAttemptToReactivateWindow, http://crbug.com/615114.
void WmShellMus::OnWindowTreeFocusChanged(::mus::Window* gained_focus,
                                          ::mus::Window* lost_focus) {
  WmWindowMus* gained_active = GetToplevelAncestor(gained_focus);
  WmWindowMus* lost_active = GetToplevelAncestor(gained_focus);
  if (gained_active == lost_active)
    return;

  FOR_EACH_OBSERVER(WmActivationObserver, activation_observers_,
                    OnWindowActivated(gained_active, lost_active));
}

void WmShellMus::OnWillDestroyClient(::mus::WindowTreeClient* client) {
  DCHECK_EQ(client, client_);
  RemoveClientObserver();
}

#if defined(OS_CHROMEOS)
void WmShellMus::ToggleIgnoreExternalKeyboard() {
  NOTIMPLEMENTED();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace mus
}  // namespace ash
