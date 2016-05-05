// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/aura/wm_globals_aura.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/wm_activation_observer.h"
#include "ash/wm/common/wm_display_observer.h"
#include "ash/wm/common/wm_overview_mode_observer.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/window_util.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/client/focus_client.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace wm {

WmGlobalsAura::WmGlobalsAura() {
  WmGlobals::Set(this);
  Shell::GetInstance()->AddShellObserver(this);
}

WmGlobalsAura::~WmGlobalsAura() {
  WmGlobals::Set(nullptr);
  if (added_activation_observer_) {
    aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
  }
  if (added_display_observer_)
    Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);

  Shell::GetInstance()->RemoveShellObserver(this);
}

WmWindow* WmGlobalsAura::GetFocusedWindow() {
  return WmWindowAura::Get(
      aura::client::GetFocusClient(Shell::GetPrimaryRootWindow())
          ->GetFocusedWindow());
}

WmWindow* WmGlobalsAura::GetActiveWindow() {
  return WmWindowAura::Get(wm::GetActiveWindow());
}

WmWindow* WmGlobalsAura::GetRootWindowForDisplayId(int64_t display_id) {
  return WmWindowAura::Get(Shell::GetInstance()
                               ->window_tree_host_manager()
                               ->GetRootWindowForDisplayId(display_id));
}

WmWindow* WmGlobalsAura::GetRootWindowForNewWindows() {
  return WmWindowAura::Get(Shell::GetTargetRootWindow());
}

std::vector<WmWindow*> WmGlobalsAura::GetMruWindowListIgnoreModals() {
  const std::vector<aura::Window*> windows = ash::Shell::GetInstance()
                                                 ->mru_window_tracker()
                                                 ->BuildWindowListIgnoreModal();
  std::vector<WmWindow*> wm_windows(windows.size());
  for (size_t i = 0; i < windows.size(); ++i)
    wm_windows[i] = WmWindowAura::Get(windows[i]);
  return wm_windows;
}

bool WmGlobalsAura::IsForceMaximizeOnFirstRun() {
  return Shell::GetInstance()->delegate()->IsForceMaximizeOnFirstRun();
}

bool WmGlobalsAura::IsUserSessionBlocked() {
  return Shell::GetInstance()->session_state_delegate()->IsUserSessionBlocked();
}

bool WmGlobalsAura::IsScreenLocked() {
  return Shell::GetInstance()->session_state_delegate()->IsScreenLocked();
}

void WmGlobalsAura::LockCursor() {
  ash::Shell::GetInstance()->cursor_manager()->LockCursor();
}

void WmGlobalsAura::UnlockCursor() {
  ash::Shell::GetInstance()->cursor_manager()->UnlockCursor();
}

std::vector<WmWindow*> WmGlobalsAura::GetAllRootWindows() {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::vector<WmWindow*> wm_windows(root_windows.size());
  for (size_t i = 0; i < root_windows.size(); ++i)
    wm_windows[i] = WmWindowAura::Get(root_windows[i]);
  return wm_windows;
}

void WmGlobalsAura::RecordUserMetricsAction(WmUserMetricsAction action) {
  return Shell::GetInstance()->metrics()->RecordUserMetricsAction(action);
}

std::unique_ptr<WindowResizer> WmGlobalsAura::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  return base::WrapUnique(
      DragWindowResizer::Create(next_window_resizer.release(), window_state));
}

bool WmGlobalsAura::IsOverviewModeSelecting() {
  WindowSelectorController* window_selector_controller =
      Shell::GetInstance()->window_selector_controller();
  return window_selector_controller &&
         window_selector_controller->IsSelecting();
}

bool WmGlobalsAura::IsOverviewModeRestoringMinimizedWindows() {
  WindowSelectorController* window_selector_controller =
      Shell::GetInstance()->window_selector_controller();
  return window_selector_controller &&
         window_selector_controller->IsRestoringMinimizedWindows();
}

void WmGlobalsAura::AddActivationObserver(WmActivationObserver* observer) {
  if (!added_activation_observer_) {
    added_activation_observer_ = true;
    aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())
        ->AddObserver(this);
  }
  activation_observers_.AddObserver(observer);
}

void WmGlobalsAura::RemoveActivationObserver(WmActivationObserver* observer) {
  activation_observers_.RemoveObserver(observer);
}

void WmGlobalsAura::AddDisplayObserver(WmDisplayObserver* observer) {
  if (!added_display_observer_) {
    added_display_observer_ = true;
    Shell::GetInstance()->window_tree_host_manager()->AddObserver(this);
  }
  display_observers_.AddObserver(observer);
}

void WmGlobalsAura::RemoveDisplayObserver(WmDisplayObserver* observer) {
  display_observers_.RemoveObserver(observer);
}

void WmGlobalsAura::AddOverviewModeObserver(WmOverviewModeObserver* observer) {
  overview_mode_observers_.AddObserver(observer);
}

void WmGlobalsAura::RemoveOverviewModeObserver(
    WmOverviewModeObserver* observer) {
  overview_mode_observers_.RemoveObserver(observer);
}

void WmGlobalsAura::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  FOR_EACH_OBSERVER(WmActivationObserver, activation_observers_,
                    OnWindowActivated(WmWindowAura::Get(gained_active),
                                      WmWindowAura::Get(lost_active)));
}

void WmGlobalsAura::OnDisplayConfigurationChanged() {
  FOR_EACH_OBSERVER(WmDisplayObserver, display_observers_,
                    OnDisplayConfigurationChanged());
}

void WmGlobalsAura::OnOverviewModeEnded() {
  FOR_EACH_OBSERVER(WmOverviewModeObserver, overview_mode_observers_,
                    OnOverviewModeEnded());
}

}  // namespace wm
}  // namespace ash
