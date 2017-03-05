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
#include "ash/common/wm_window.h"
#include "ash/mus/accelerators/accelerator_controller_delegate_mus.h"
#include "ash/mus/accelerators/accelerator_controller_registrar.h"
#include "ash/mus/bridge/immersive_handler_factory_mus.h"
#include "ash/mus/bridge/workspace_event_handler_mus.h"
#include "ash/mus/drag_window_resizer.h"
#include "ash/mus/keyboard_ui_mus.h"
#include "ash/mus/screen_mus.h"
#include "ash/mus/window_manager.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shared/immersive_fullscreen_controller.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/wm/window_util.h"
#include "base/memory/ptr_util.h"
#include "components/user_manager/user_info_impl.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/views/mus/pointer_watcher_event_router.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/focus_controller.h"

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
  int NumberOfLoggedInUsers() const override { return 1; }
  bool IsActiveUserSessionStarted() const override { return true; }
  bool CanLockScreen() const override {
    // The Chrome OS session_manager process currently rejects screen-lock
    // requests due to no user being logged in.
    return false;
  }
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

WmShellMus::WmShellMus(
    WmWindow* primary_root_window,
    std::unique_ptr<ShellDelegate> shell_delegate,
    WindowManager* window_manager,
    views::PointerWatcherEventRouter* pointer_watcher_event_router,
    bool create_session_state_delegate_stub)
    : WmShell(std::move(shell_delegate)),
      window_manager_(window_manager),
      primary_root_window_(primary_root_window),
      pointer_watcher_event_router_(pointer_watcher_event_router) {
  if (create_session_state_delegate_stub)
    session_state_delegate_ = base::MakeUnique<SessionStateDelegateStub>();
  DCHECK(primary_root_window_);
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

  SetKeyboardUI(KeyboardUIMus::Create(window_manager_->connector()));
}

WmShellMus::~WmShellMus() {
  WmShell::Set(nullptr);
}

// static
WmShellMus* WmShellMus::Get() {
  return static_cast<WmShellMus*>(WmShell::Get());
}

RootWindowController* WmShellMus::GetRootWindowControllerWithDisplayId(
    int64_t id) {
  for (RootWindowController* root_window_controller :
       RootWindowController::root_window_controllers()) {
    RootWindowSettings* settings =
        GetRootWindowSettings(root_window_controller->GetRootWindow());
    DCHECK(settings);
    if (settings->display_id == id)
      return root_window_controller;
  }
  NOTREACHED();
  return nullptr;
}

aura::WindowTreeClient* WmShellMus::window_tree_client() {
  return window_manager_->window_tree_client();
}

void WmShellMus::Initialize(
    const scoped_refptr<base::SequencedWorkerPool>& pool) {
  WmShell::Initialize(pool);
}

void WmShellMus::Shutdown() {
  WmShell::Shutdown();

  window_manager_->DeleteAllRootWindowControllers();
}

bool WmShellMus::IsRunningInMash() const {
  return true;
}

WmWindow* WmShellMus::NewWindow(ui::wm::WindowType window_type,
                                ui::LayerType layer_type) {
  aura::Window* window = new aura::Window(nullptr);
  window->SetType(window_type);
  window->Init(layer_type);
  return WmWindow::Get(window);
}

WmWindow* WmShellMus::GetFocusedWindow() {
  // TODO: remove as both WmShells use same implementation.
  return WmWindow::Get(
      aura::client::GetFocusClient(Shell::GetPrimaryRootWindow())
          ->GetFocusedWindow());
}

WmWindow* WmShellMus::GetActiveWindow() {
  // TODO: remove as both WmShells use same implementation.
  return WmWindow::Get(wm::GetActiveWindow());
}

WmWindow* WmShellMus::GetCaptureWindow() {
  // TODO: remove as both WmShells use same implementation.
  return WmWindow::Get(::wm::CaptureController::Get()->GetCaptureWindow());
}

WmWindow* WmShellMus::GetPrimaryRootWindow() {
  // NOTE: This is called before the RootWindowController has been created, so
  // it can't call through to RootWindowController to get all windows.
  return primary_root_window_;
}

WmWindow* WmShellMus::GetRootWindowForDisplayId(int64_t display_id) {
  RootWindowController* root_window_controller =
      GetRootWindowControllerWithDisplayId(display_id);
  DCHECK(root_window_controller);
  return WmWindow::Get(root_window_controller->GetRootWindow());
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
  window_manager_->screen()->SetWorkAreaInsets(window->aura_window(), insets);
}

bool WmShellMus::IsPinned() {
  // TODO: http://crbug.com/622486.
  NOTIMPLEMENTED();
  return false;
}

void WmShellMus::SetPinnedWindow(WmWindow* window) {
  // TODO: http://crbug.com/622486.
  NOTIMPLEMENTED();
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
  std::vector<WmWindow*> root_windows;
  for (RootWindowController* root_window_controller :
       RootWindowController::root_window_controllers()) {
    root_windows.push_back(root_window_controller->GetWindow());
  }
  return root_windows;
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
      WmWindow::GetAuraWindow(workspace_window));
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
  return session_state_delegate_
             ? session_state_delegate_.get()
             : Shell::GetInstance()->session_state_delegate();
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

void WmShellMus::ToggleIgnoreExternalKeyboard() {
  NOTIMPLEMENTED();
}

void WmShellMus::SetLaserPointerEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

void WmShellMus::SetPartialMagnifierEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

void WmShellMus::CreatePointerWatcherAdapter() {
  // Only needed in WmShellAura, which has specific creation order.
}

void WmShellMus::CreatePrimaryHost() {}

void WmShellMus::InitHosts(const ShellInitParams& init_params) {
  window_manager_->CreatePrimaryRootWindowController(
      base::WrapUnique(init_params.primary_window_tree_host));
}
}  // namespace mus
}  // namespace ash
