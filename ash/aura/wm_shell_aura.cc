// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_shell_aura.h"

#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_controller_delegate_aura.h"
#include "ash/aura/key_event_watcher_aura.h"
#include "ash/aura/pointer_watcher_adapter.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/keyboard/keyboard_ui.h"
#include "ash/laser/laser_pointer_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/metrics/task_switch_metrics_recorder.h"
#include "ash/public/cpp/config.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shared/immersive_fullscreen_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_observer.h"
#include "ash/touch/touch_uma.h"
#include "ash/virtual_keyboard_controller.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/maximize_mode/maximize_mode_event_handler_aura.h"
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/window_cycle_event_filter_aura.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_handler_aura.h"
#include "ash/wm_display_observer.h"
#include "ash/wm_window.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/env.h"
#include "ui/display/manager/display_manager.h"

#if defined(USE_X11)
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_x11.h"
#endif

#if defined(USE_OZONE)
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_ozone.h"
#endif

namespace ash {

WmShellAura::WmShellAura() {}

WmShellAura::~WmShellAura() {}

// static
WmShellAura* WmShellAura::Get() {
  CHECK(!WmShell::Get()->IsRunningInMash());
  return static_cast<WmShellAura*>(WmShell::Get());
}

void WmShellAura::Shutdown() {
  if (added_display_observer_)
    Shell::Get()->window_tree_host_manager()->RemoveObserver(this);

  pointer_watcher_adapter_.reset();

  WmShell::Shutdown();

  Shell::Get()->window_tree_host_manager()->Shutdown();
}

bool WmShellAura::IsRunningInMash() const {
  return false;
}

Config WmShellAura::GetAshConfig() const {
  return Config::CLASSIC;
}

WmWindow* WmShellAura::GetPrimaryRootWindow() {
  return WmWindow::Get(
      Shell::Get()->window_tree_host_manager()->GetPrimaryRootWindow());
}

WmWindow* WmShellAura::GetRootWindowForDisplayId(int64_t display_id) {
  return WmWindow::Get(
      Shell::Get()->window_tree_host_manager()->GetRootWindowForDisplayId(
          display_id));
}

const display::ManagedDisplayInfo& WmShellAura::GetDisplayInfo(
    int64_t display_id) const {
  return Shell::Get()->display_manager()->GetDisplayInfo(display_id);
}

bool WmShellAura::IsActiveDisplayId(int64_t display_id) const {
  return Shell::Get()->display_manager()->IsActiveDisplayId(display_id);
}

display::Display WmShellAura::GetFirstDisplay() const {
  return Shell::Get()->display_manager()->software_mirroring_display_list()[0];
}

bool WmShellAura::IsInUnifiedMode() const {
  return Shell::Get()->display_manager()->IsInUnifiedMode();
}

bool WmShellAura::IsInUnifiedModeIgnoreMirroring() const {
  return Shell::Get()
             ->display_manager()
             ->current_default_multi_display_mode() ==
         display::DisplayManager::UNIFIED;
}

void WmShellAura::SetDisplayWorkAreaInsets(WmWindow* window,
                                           const gfx::Insets& insets) {
  Shell::Get()
      ->window_tree_host_manager()
      ->UpdateWorkAreaOfDisplayNearestWindow(window->aura_window(), insets);
}

void WmShellAura::LockCursor() {
  Shell::Get()->cursor_manager()->LockCursor();
}

void WmShellAura::UnlockCursor() {
  Shell::Get()->cursor_manager()->UnlockCursor();
}

bool WmShellAura::IsMouseEventsEnabled() {
  return Shell::Get()->cursor_manager()->IsMouseEventsEnabled();
}

std::vector<WmWindow*> WmShellAura::GetAllRootWindows() {
  aura::Window::Windows root_windows =
      Shell::Get()->window_tree_host_manager()->GetAllRootWindows();
  std::vector<WmWindow*> wm_windows(root_windows.size());
  for (size_t i = 0; i < root_windows.size(); ++i)
    wm_windows[i] = WmWindow::Get(root_windows[i]);
  return wm_windows;
}

void WmShellAura::RecordUserMetricsAction(UserMetricsAction action) {
  Shell::Get()->metrics()->RecordUserMetricsAction(action);
}

void WmShellAura::RecordGestureAction(GestureActionType action) {
  TouchUMA::GetInstance()->RecordGestureAction(action);
}

void WmShellAura::RecordTaskSwitchMetric(TaskSwitchSource source) {
  Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(source);
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

std::unique_ptr<KeyboardUI> WmShellAura::CreateKeyboardUI() {
  return KeyboardUI::Create();
}

std::unique_ptr<KeyEventWatcher> WmShellAura::CreateKeyEventWatcher() {
  return base::MakeUnique<KeyEventWatcherAura>();
}

SessionStateDelegate* WmShellAura::GetSessionStateDelegate() {
  return Shell::Get()->session_state_delegate();
}

void WmShellAura::AddDisplayObserver(WmDisplayObserver* observer) {
  if (!added_display_observer_) {
    added_display_observer_ = true;
    Shell::Get()->window_tree_host_manager()->AddObserver(this);
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
  Shell::Get()->virtual_keyboard_controller()->ToggleIgnoreExternalKeyboard();
}

void WmShellAura::SetLaserPointerEnabled(bool enabled) {
  Shell::Get()->laser_pointer_controller()->SetEnabled(enabled);
}

void WmShellAura::SetPartialMagnifierEnabled(bool enabled) {
  Shell::Get()->partial_magnification_controller()->SetEnabled(enabled);
}

void WmShellAura::CreatePointerWatcherAdapter() {
  pointer_watcher_adapter_ = base::MakeUnique<PointerWatcherAdapter>();
}

void WmShellAura::CreatePrimaryHost() {
  Shell::Get()->window_tree_host_manager()->Start();
  AshWindowTreeHostInitParams ash_init_params;
  Shell::Get()->window_tree_host_manager()->CreatePrimaryHost(ash_init_params);
}

void WmShellAura::InitHosts(const ShellInitParams& init_params) {
  Shell::Get()->window_tree_host_manager()->InitHosts();
}

std::unique_ptr<AcceleratorController>
WmShellAura::CreateAcceleratorController() {
  DCHECK(!accelerator_controller_delegate_);
  accelerator_controller_delegate_ =
      base::MakeUnique<AcceleratorControllerDelegateAura>();
  return base::MakeUnique<AcceleratorController>(
      accelerator_controller_delegate_.get(), nullptr);
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
