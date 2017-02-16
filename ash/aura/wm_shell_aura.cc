// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_shell_aura.h"

#include <utility>

#include "ash/aura/key_event_watcher_aura.h"
#include "ash/aura/pointer_watcher_adapter.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/shell_observer.h"
#include "ash/common/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/common/wm_display_observer.h"
#include "ash/common/wm_window.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/laser/laser_pointer_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/metrics/task_switch_metrics_recorder.h"
#include "ash/shared/immersive_fullscreen_controller.h"
#include "ash/shell.h"
#include "ash/touch/touch_uma.h"
#include "ash/virtual_keyboard_controller.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/maximize_mode/maximize_mode_event_handler_aura.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_cycle_event_filter_aura.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_handler_aura.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/display/manager/display_manager.h"

#if defined(USE_X11)
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_x11.h"
#endif

#if defined(USE_OZONE)
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_ozone.h"
#endif

namespace ash {

WmShellAura::WmShellAura(std::unique_ptr<ShellDelegate> shell_delegate)
    : WmShell(std::move(shell_delegate)) {
  WmShell::Set(this);
}

WmShellAura::~WmShellAura() {
  WmShell::Set(nullptr);
}

void WmShellAura::Shutdown() {
  if (added_display_observer_)
    Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);

  pointer_watcher_adapter_.reset();

  WmShell::Shutdown();

  Shell::GetInstance()->window_tree_host_manager()->Shutdown();
}

bool WmShellAura::IsRunningInMash() const {
  return false;
}

WmWindow* WmShellAura::NewWindow(ui::wm::WindowType window_type,
                                 ui::LayerType layer_type) {
  aura::Window* aura_window = new aura::Window(nullptr);
  aura_window->SetType(window_type);
  aura_window->Init(layer_type);
  return WmWindow::Get(aura_window);
}

WmWindow* WmShellAura::GetFocusedWindow() {
  return WmWindow::Get(
      aura::client::GetFocusClient(Shell::GetPrimaryRootWindow())
          ->GetFocusedWindow());
}

WmWindow* WmShellAura::GetActiveWindow() {
  return WmWindow::Get(wm::GetActiveWindow());
}

WmWindow* WmShellAura::GetCaptureWindow() {
  // Ash shares capture client among all RootWindowControllers, so we need only
  // check the primary root.
  return WmWindow::Get(
      aura::client::GetCaptureWindow(Shell::GetPrimaryRootWindow()));
}

WmWindow* WmShellAura::GetPrimaryRootWindow() {
  return WmWindow::Get(
      Shell::GetInstance()->window_tree_host_manager()->GetPrimaryRootWindow());
}

WmWindow* WmShellAura::GetRootWindowForDisplayId(int64_t display_id) {
  return WmWindow::Get(Shell::GetInstance()
                           ->window_tree_host_manager()
                           ->GetRootWindowForDisplayId(display_id));
}

const display::ManagedDisplayInfo& WmShellAura::GetDisplayInfo(
    int64_t display_id) const {
  return Shell::GetInstance()->display_manager()->GetDisplayInfo(display_id);
}

bool WmShellAura::IsActiveDisplayId(int64_t display_id) const {
  return Shell::GetInstance()->display_manager()->IsActiveDisplayId(display_id);
}

display::Display WmShellAura::GetFirstDisplay() const {
  return Shell::GetInstance()
      ->display_manager()
      ->software_mirroring_display_list()[0];
}

bool WmShellAura::IsInUnifiedMode() const {
  return Shell::GetInstance()->display_manager()->IsInUnifiedMode();
}

bool WmShellAura::IsInUnifiedModeIgnoreMirroring() const {
  return Shell::GetInstance()
             ->display_manager()
             ->current_default_multi_display_mode() ==
         display::DisplayManager::UNIFIED;
}

bool WmShellAura::IsForceMaximizeOnFirstRun() {
  return delegate()->IsForceMaximizeOnFirstRun();
}

void WmShellAura::SetDisplayWorkAreaInsets(WmWindow* window,
                                           const gfx::Insets& insets) {
  Shell::GetInstance()
      ->window_tree_host_manager()
      ->UpdateWorkAreaOfDisplayNearestWindow(window->aura_window(), insets);
}

bool WmShellAura::IsPinned() {
  return Shell::GetInstance()->screen_pinning_controller()->IsPinned();
}

void WmShellAura::SetPinnedWindow(WmWindow* window) {
  return Shell::GetInstance()->screen_pinning_controller()->SetPinnedWindow(
      window);
}

void WmShellAura::LockCursor() {
  Shell::GetInstance()->cursor_manager()->LockCursor();
}

void WmShellAura::UnlockCursor() {
  Shell::GetInstance()->cursor_manager()->UnlockCursor();
}

bool WmShellAura::IsMouseEventsEnabled() {
  return Shell::GetInstance()->cursor_manager()->IsMouseEventsEnabled();
}

std::vector<WmWindow*> WmShellAura::GetAllRootWindows() {
  aura::Window::Windows root_windows =
      Shell::GetInstance()->window_tree_host_manager()->GetAllRootWindows();
  std::vector<WmWindow*> wm_windows(root_windows.size());
  for (size_t i = 0; i < root_windows.size(); ++i)
    wm_windows[i] = WmWindow::Get(root_windows[i]);
  return wm_windows;
}

void WmShellAura::RecordUserMetricsAction(UserMetricsAction action) {
  Shell::GetInstance()->metrics()->RecordUserMetricsAction(action);
}

void WmShellAura::RecordGestureAction(GestureActionType action) {
  TouchUMA::GetInstance()->RecordGestureAction(action);
}

void WmShellAura::RecordTaskSwitchMetric(TaskSwitchSource source) {
  Shell::GetInstance()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
      source);
}

std::unique_ptr<WindowResizer> WmShellAura::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  return base::WrapUnique(
      DragWindowResizer::Create(next_window_resizer.release(), window_state));
}

std::unique_ptr<WindowCycleEventFilter>
WmShellAura::CreateWindowCycleEventFilter() {
  return base::MakeUnique<WindowCycleEventFilterAura>();
}

std::unique_ptr<wm::MaximizeModeEventHandler>
WmShellAura::CreateMaximizeModeEventHandler() {
  return base::WrapUnique(new wm::MaximizeModeEventHandlerAura);
}

std::unique_ptr<WorkspaceEventHandler> WmShellAura::CreateWorkspaceEventHandler(
    WmWindow* workspace_window) {
  return base::MakeUnique<WorkspaceEventHandlerAura>(workspace_window);
}

std::unique_ptr<ScopedDisableInternalMouseAndKeyboard>
WmShellAura::CreateScopedDisableInternalMouseAndKeyboard() {
#if defined(USE_X11)
  return base::WrapUnique(new ScopedDisableInternalMouseAndKeyboardX11);
#elif defined(USE_OZONE)
  return base::WrapUnique(new ScopedDisableInternalMouseAndKeyboardOzone);
#endif
  return nullptr;
}

std::unique_ptr<ImmersiveFullscreenController>
WmShellAura::CreateImmersiveFullscreenController() {
  return base::MakeUnique<ImmersiveFullscreenController>();
}

std::unique_ptr<KeyEventWatcher> WmShellAura::CreateKeyEventWatcher() {
  return base::MakeUnique<KeyEventWatcherAura>();
}

void WmShellAura::OnOverviewModeStarting() {
  for (auto& observer : *shell_observers())
    observer.OnOverviewModeStarting();
}

void WmShellAura::OnOverviewModeEnded() {
  for (auto& observer : *shell_observers())
    observer.OnOverviewModeEnded();
}

SessionStateDelegate* WmShellAura::GetSessionStateDelegate() {
  return Shell::GetInstance()->session_state_delegate();
}

void WmShellAura::AddDisplayObserver(WmDisplayObserver* observer) {
  if (!added_display_observer_) {
    added_display_observer_ = true;
    Shell::GetInstance()->window_tree_host_manager()->AddObserver(this);
  }
  display_observers_.AddObserver(observer);
}

void WmShellAura::RemoveDisplayObserver(WmDisplayObserver* observer) {
  display_observers_.RemoveObserver(observer);
}

void WmShellAura::AddPointerWatcher(views::PointerWatcher* watcher,
                                    views::PointerWatcherEventTypes events) {
  pointer_watcher_adapter_->AddPointerWatcher(watcher, events);
}

void WmShellAura::RemovePointerWatcher(views::PointerWatcher* watcher) {
  pointer_watcher_adapter_->RemovePointerWatcher(watcher);
}

bool WmShellAura::IsTouchDown() {
  return aura::Env::GetInstance()->is_touch_down();
}

void WmShellAura::ToggleIgnoreExternalKeyboard() {
  Shell::GetInstance()
      ->virtual_keyboard_controller()
      ->ToggleIgnoreExternalKeyboard();
}

void WmShellAura::SetLaserPointerEnabled(bool enabled) {
  Shell::GetInstance()->laser_pointer_controller()->SetEnabled(enabled);
}

void WmShellAura::SetPartialMagnifierEnabled(bool enabled) {
  Shell::GetInstance()->partial_magnification_controller()->SetEnabled(enabled);
}

void WmShellAura::CreatePointerWatcherAdapter() {
  pointer_watcher_adapter_ = base::MakeUnique<PointerWatcherAdapter>();
}

void WmShellAura::CreatePrimaryHost() {
  Shell::GetInstance()->window_tree_host_manager()->Start();
  AshWindowTreeHostInitParams ash_init_params;
  Shell::GetInstance()->window_tree_host_manager()->CreatePrimaryHost(
      ash_init_params);
}

void WmShellAura::InitHosts(const ShellInitParams& init_params) {
  Shell::GetInstance()->window_tree_host_manager()->InitHosts();
}

void WmShellAura::SessionStateChanged(session_manager::SessionState state) {
  // Create the shelf if necessary.
  WmShell::SessionStateChanged(state);

  // Recreate the keyboard after initial login and after multiprofile login.
  if (state == session_manager::SessionState::ACTIVE)
    Shell::GetInstance()->CreateKeyboard();
}

void WmShellAura::OnDisplayConfigurationChanging() {
  for (auto& observer : display_observers_)
    observer.OnDisplayConfigurationChanging();
}

void WmShellAura::OnDisplayConfigurationChanged() {
  for (auto& observer : display_observers_)
    observer.OnDisplayConfigurationChanged();
}

}  // namespace ash
