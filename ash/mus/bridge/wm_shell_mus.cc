// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_shell_mus.h"

#include <utility>

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/key_event_watcher.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/shell_observer.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wm/maximize_mode/maximize_mode_event_handler.h"
#include "ash/common/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/window_cycle_event_filter.h"
#include "ash/common/wm/window_resizer.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/mus/accelerators/accelerator_controller_delegate_mus.h"
#include "ash/mus/accelerators/accelerator_controller_registrar.h"
#include "ash/mus/bridge/immersive_handler_factory_mus.h"
#include "ash/mus/bridge/wm_root_window_controller_mus.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/bridge/workspace_event_handler_mus.h"
#include "ash/mus/drag_window_resizer.h"
#include "ash/mus/keyboard_ui_mus.h"
#include "ash/mus/root_window_controller.h"
#include "ash/mus/window_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shared/immersive_fullscreen_controller.h"
#include "base/memory/ptr_util.h"
#include "components/user_manager/user_info_impl.h"
#include "services/ui/common/util.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/views/mus/pointer_watcher_event_router.h"

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

WmShellMus::WmShellMus(
    std::unique_ptr<ShellDelegate> shell_delegate,
    WindowManager* window_manager,
    views::PointerWatcherEventRouter* pointer_watcher_event_router)
    : WmShell(std::move(shell_delegate)),
      window_manager_(window_manager),
      pointer_watcher_event_router_(pointer_watcher_event_router),
      session_state_delegate_(new SessionStateDelegateStub) {
  window_tree_client()->AddObserver(this);
  WmShell::Set(this);

  uint16_t accelerator_namespace_id = 0u;
  const bool add_result =
      window_manager->GetNextAcceleratorNamespaceId(&accelerator_namespace_id);
  // WmShellMus is created early on, so that this should always succeed.
  DCHECK(add_result);
  accelerator_controller_delegate_.reset(
      new AcceleratorControllerDelegateMus(window_manager_));
  accelerator_controller_registrar_.reset(new AcceleratorControllerRegistrar(
      window_manager_, accelerator_namespace_id));
  SetAcceleratorController(base::MakeUnique<AcceleratorController>(
      accelerator_controller_delegate_.get(),
      accelerator_controller_registrar_.get()));
  immersive_handler_factory_.reset(new ImmersiveHandlerFactoryMus);

  CreateMaximizeModeController();

  CreateMruWindowTracker();

  SetSystemTrayDelegate(
      base::WrapUnique(delegate()->CreateSystemTrayDelegate()));

  SetKeyboardUI(KeyboardUIMus::Create(window_manager_->connector()));

  wallpaper_delegate()->InitializeWallpaper();
}

WmShellMus::~WmShellMus() {
  // This order mirrors that of Shell.

  // Destroy maximize mode controller early on since it has some observers which
  // need to be removed.
  DeleteMaximizeModeController();
  DeleteToastManager();
  DeleteSystemTrayDelegate();
  // Has to happen before ~MruWindowTracker.
  DeleteWindowCycleController();
  DeleteWindowSelectorController();
  DeleteMruWindowTracker();
  if (window_tree_client())
    window_tree_client()->RemoveObserver(this);
  WmShell::Set(nullptr);
}

// static
WmShellMus* WmShellMus::Get() {
  return static_cast<WmShellMus*>(WmShell::Get());
}

void WmShellMus::AddRootWindowController(
    WmRootWindowControllerMus* controller) {
  root_window_controllers_.push_back(controller);
  // The first root window will be the initial root for new windows.
  if (!GetRootWindowForNewWindows())
    set_root_window_for_new_windows(controller->GetWindow());
}

void WmShellMus::RemoveRootWindowController(
    WmRootWindowControllerMus* controller) {
  auto iter = std::find(root_window_controllers_.begin(),
                        root_window_controllers_.end(), controller);
  DCHECK(iter != root_window_controllers_.end());
  root_window_controllers_.erase(iter);
}

// static
WmWindowMus* WmShellMus::GetToplevelAncestor(ui::Window* window) {
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

bool WmShellMus::IsRunningInMash() const {
  return true;
}

WmWindow* WmShellMus::NewWindow(ui::wm::WindowType window_type,
                                ui::LayerType layer_type) {
  WmWindowMus* window = WmWindowMus::Get(window_tree_client()->NewWindow());
  window->set_wm_window_type(window_type);
  // TODO(sky): support layer_type.
  NOTIMPLEMENTED();
  return window;
}

WmWindow* WmShellMus::GetFocusedWindow() {
  return WmWindowMus::Get(window_tree_client()->GetFocusedWindow());
}

WmWindow* WmShellMus::GetActiveWindow() {
  return GetToplevelAncestor(window_tree_client()->GetFocusedWindow());
}

WmWindow* WmShellMus::GetCaptureWindow() {
  return WmWindowMus::Get(window_tree_client()->GetCaptureWindow());
}

WmWindow* WmShellMus::GetPrimaryRootWindow() {
  return root_window_controllers_[0]->GetWindow();
}

WmWindow* WmShellMus::GetRootWindowForDisplayId(int64_t display_id) {
  return GetRootWindowControllerWithDisplayId(display_id)->GetWindow();
}

const display::ManagedDisplayInfo& WmShellMus::GetDisplayInfo(
    int64_t display_id) const {
  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  static display::ManagedDisplayInfo fake_info;
  return fake_info;
}

bool WmShellMus::IsActiveDisplayId(int64_t display_id) const {
  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return true;
}

display::Display WmShellMus::GetFirstDisplay() const {
  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return display::Screen::GetScreen()->GetPrimaryDisplay();
}

bool WmShellMus::IsInUnifiedMode() const {
  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return false;
}

bool WmShellMus::IsInUnifiedModeIgnoreMirroring() const {
  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return false;
}

bool WmShellMus::IsForceMaximizeOnFirstRun() {
  NOTIMPLEMENTED();
  return false;
}

void WmShellMus::SetDisplayWorkAreaInsets(WmWindow* window,
                                          const gfx::Insets& insets) {
  RootWindowController* root_window_controller =
      GetRootWindowControllerWithDisplayId(
          WmWindowMus::GetMusWindow(window)->display_id())
          ->root_window_controller();
  root_window_controller->SetWorkAreaInests(insets);
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
  // TODO: http::/crbug.com/637853
  NOTIMPLEMENTED();
}

void WmShellMus::UnlockCursor() {
  // TODO: http::/crbug.com/637853
  NOTIMPLEMENTED();
}

bool WmShellMus::IsMouseEventsEnabled() {
  // TODO: http::/crbug.com/637853
  NOTIMPLEMENTED();
  return true;
}

std::vector<WmWindow*> WmShellMus::GetAllRootWindows() {
  std::vector<WmWindow*> wm_windows(root_window_controllers_.size());
  for (size_t i = 0; i < root_window_controllers_.size(); ++i)
    wm_windows[i] = root_window_controllers_[i]->GetWindow();
  return wm_windows;
}

void WmShellMus::RecordGestureAction(GestureActionType action) {
  // TODO: http://crbug.com/616581.
  NOTIMPLEMENTED();
}

void WmShellMus::RecordUserMetricsAction(UserMetricsAction action) {
  // TODO: http://crbug.com/616581.
  NOTIMPLEMENTED();
}

void WmShellMus::RecordTaskSwitchMetric(TaskSwitchSource source) {
  // TODO: http://crbug.com/616581.
  NOTIMPLEMENTED();
}

std::unique_ptr<WindowResizer> WmShellMus::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  return base::MakeUnique<DragWindowResizer>(std::move(next_window_resizer),
                                             window_state);
}

std::unique_ptr<WindowCycleEventFilter>
WmShellMus::CreateWindowCycleEventFilter() {
  // TODO: implement me, http://crbug.com/629191.
  return nullptr;
}

std::unique_ptr<wm::MaximizeModeEventHandler>
WmShellMus::CreateMaximizeModeEventHandler() {
  // TODO: need support for window manager to get events before client:
  // http://crbug.com/624157.
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<ScopedDisableInternalMouseAndKeyboard>
WmShellMus::CreateScopedDisableInternalMouseAndKeyboard() {
  // TODO: needs implementation for mus, http://crbug.com/624967.
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<WorkspaceEventHandler> WmShellMus::CreateWorkspaceEventHandler(
    WmWindow* workspace_window) {
  return base::MakeUnique<WorkspaceEventHandlerMus>(
      WmWindowMus::GetMusWindow(workspace_window));
}

std::unique_ptr<ImmersiveFullscreenController>
WmShellMus::CreateImmersiveFullscreenController() {
  return base::MakeUnique<ImmersiveFullscreenController>();
}

std::unique_ptr<KeyEventWatcher> WmShellMus::CreateKeyEventWatcher() {
  // TODO: needs implementation for mus, http://crbug.com/649600.
  NOTIMPLEMENTED();
  return std::unique_ptr<KeyEventWatcher>();
}

void WmShellMus::OnOverviewModeStarting() {
  for (auto& observer : *shell_observers())
    observer.OnOverviewModeStarting();
}

void WmShellMus::OnOverviewModeEnded() {
  for (auto& observer : *shell_observers())
    observer.OnOverviewModeEnded();
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

void WmShellMus::AddPointerWatcher(views::PointerWatcher* watcher,
                                   views::PointerWatcherEventTypes events) {
  // TODO: implement drags for mus pointer watcher, http://crbug.com/641164.
  // NOTIMPLEMENTED drags for mus pointer watcher.
  pointer_watcher_event_router_->AddPointerWatcher(
      watcher, events == views::PointerWatcherEventTypes::MOVES);
}

void WmShellMus::RemovePointerWatcher(views::PointerWatcher* watcher) {
  pointer_watcher_event_router_->RemovePointerWatcher(watcher);
}

bool WmShellMus::IsTouchDown() {
  // TODO: implement me, http://crbug.com/634967.
  // NOTIMPLEMENTED is too spammy here.
  return false;
}

#if defined(OS_CHROMEOS)
void WmShellMus::ToggleIgnoreExternalKeyboard() {
  NOTIMPLEMENTED();
}

void WmShellMus::SetLaserPointerEnabled(bool enabled) {
  NOTIMPLEMENTED();
}
#endif  // defined(OS_CHROMEOS)

ui::WindowTreeClient* WmShellMus::window_tree_client() {
  return window_manager_->window_tree_client();
}

// static
bool WmShellMus::IsActivationParent(ui::Window* window) {
  return window && IsActivatableShellWindowId(
                       WmWindowMus::Get(window)->GetShellWindowId());
}

// TODO: support OnAttemptToReactivateWindow, http://crbug.com/615114.
void WmShellMus::OnWindowTreeFocusChanged(ui::Window* gained_focus,
                                          ui::Window* lost_focus) {
  WmWindow* gained_active = GetToplevelAncestor(gained_focus);
  if (gained_active)
    set_root_window_for_new_windows(gained_active->GetRootWindow());

  WmWindow* lost_active = GetToplevelAncestor(lost_focus);
  if (gained_active == lost_active)
    return;

  for (auto& observer : activation_observers_)
    observer.OnWindowActivated(gained_active, lost_active);
}

void WmShellMus::OnDidDestroyClient(ui::WindowTreeClient* client) {
  DCHECK_EQ(window_tree_client(), client);
  client->RemoveObserver(this);
}

}  // namespace mus
}  // namespace ash
