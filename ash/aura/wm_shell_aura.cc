// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_shell_aura.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/common/wm_display_observer.h"
#include "ash/common/wm_overview_mode_observer.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/window_util.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/client/focus_client.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

WmShellAura::WmShellAura() {
  WmShell::Set(this);
  Shell::GetInstance()->AddShellObserver(this);
}

WmShellAura::~WmShellAura() {
  WmShell::Set(nullptr);
  if (added_activation_observer_) {
    aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
  }
  if (added_display_observer_)
    Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);

  Shell::GetInstance()->RemoveShellObserver(this);
}

WmWindow* WmShellAura::NewContainerWindow() {
  aura::Window* aura_window = new aura::Window(nullptr);
  aura_window->Init(ui::LAYER_NOT_DRAWN);
  return WmWindowAura::Get(aura_window);
}

WmWindow* WmShellAura::GetFocusedWindow() {
  return WmWindowAura::Get(
      aura::client::GetFocusClient(Shell::GetPrimaryRootWindow())
          ->GetFocusedWindow());
}

WmWindow* WmShellAura::GetActiveWindow() {
  return WmWindowAura::Get(wm::GetActiveWindow());
}

WmWindow* WmShellAura::GetPrimaryRootWindow() {
  return WmWindowAura::Get(Shell::GetPrimaryRootWindow());
}

WmWindow* WmShellAura::GetRootWindowForDisplayId(int64_t display_id) {
  return WmWindowAura::Get(Shell::GetInstance()
                               ->window_tree_host_manager()
                               ->GetRootWindowForDisplayId(display_id));
}

WmWindow* WmShellAura::GetRootWindowForNewWindows() {
  return WmWindowAura::Get(Shell::GetTargetRootWindow());
}

std::vector<WmWindow*> WmShellAura::GetMruWindowList() {
  return WmWindowAura::FromAuraWindows(
      Shell::GetInstance()->mru_window_tracker()->BuildMruWindowList());
}

std::vector<WmWindow*> WmShellAura::GetMruWindowListIgnoreModals() {
  return WmWindowAura::FromAuraWindows(
      Shell::GetInstance()->mru_window_tracker()->BuildWindowListIgnoreModal());
}

bool WmShellAura::IsForceMaximizeOnFirstRun() {
  return Shell::GetInstance()->delegate()->IsForceMaximizeOnFirstRun();
}

bool WmShellAura::IsUserSessionBlocked() {
  return Shell::GetInstance()->session_state_delegate()->IsUserSessionBlocked();
}

bool WmShellAura::IsScreenLocked() {
  return Shell::GetInstance()->session_state_delegate()->IsScreenLocked();
}

void WmShellAura::LockCursor() {
  Shell::GetInstance()->cursor_manager()->LockCursor();
}

void WmShellAura::UnlockCursor() {
  Shell::GetInstance()->cursor_manager()->UnlockCursor();
}

std::vector<WmWindow*> WmShellAura::GetAllRootWindows() {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::vector<WmWindow*> wm_windows(root_windows.size());
  for (size_t i = 0; i < root_windows.size(); ++i)
    wm_windows[i] = WmWindowAura::Get(root_windows[i]);
  return wm_windows;
}

void WmShellAura::RecordUserMetricsAction(wm::WmUserMetricsAction action) {
  return Shell::GetInstance()->metrics()->RecordUserMetricsAction(action);
}

std::unique_ptr<WindowResizer> WmShellAura::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  return base::WrapUnique(
      DragWindowResizer::Create(next_window_resizer.release(), window_state));
}

bool WmShellAura::IsOverviewModeSelecting() {
  WindowSelectorController* window_selector_controller =
      Shell::GetInstance()->window_selector_controller();
  return window_selector_controller &&
         window_selector_controller->IsSelecting();
}

bool WmShellAura::IsOverviewModeRestoringMinimizedWindows() {
  WindowSelectorController* window_selector_controller =
      Shell::GetInstance()->window_selector_controller();
  return window_selector_controller &&
         window_selector_controller->IsRestoringMinimizedWindows();
}

void WmShellAura::AddActivationObserver(WmActivationObserver* observer) {
  if (!added_activation_observer_) {
    added_activation_observer_ = true;
    aura::client::GetActivationClient(Shell::GetPrimaryRootWindow())
        ->AddObserver(this);
  }
  activation_observers_.AddObserver(observer);
}

void WmShellAura::RemoveActivationObserver(WmActivationObserver* observer) {
  activation_observers_.RemoveObserver(observer);
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

void WmShellAura::AddOverviewModeObserver(WmOverviewModeObserver* observer) {
  overview_mode_observers_.AddObserver(observer);
}

void WmShellAura::RemoveOverviewModeObserver(WmOverviewModeObserver* observer) {
  overview_mode_observers_.RemoveObserver(observer);
}

void WmShellAura::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  FOR_EACH_OBSERVER(WmActivationObserver, activation_observers_,
                    OnWindowActivated(WmWindowAura::Get(gained_active),
                                      WmWindowAura::Get(lost_active)));
}

void WmShellAura::OnAttemptToReactivateWindow(aura::Window* request_active,
                                              aura::Window* actual_active) {
  FOR_EACH_OBSERVER(
      WmActivationObserver, activation_observers_,
      OnAttemptToReactivateWindow(WmWindowAura::Get(request_active),
                                  WmWindowAura::Get(actual_active)));
}

void WmShellAura::OnDisplayConfigurationChanging() {
  FOR_EACH_OBSERVER(WmDisplayObserver, display_observers_,
                    OnDisplayConfigurationChanging());
}

void WmShellAura::OnDisplayConfigurationChanged() {
  FOR_EACH_OBSERVER(WmDisplayObserver, display_observers_,
                    OnDisplayConfigurationChanged());
}

void WmShellAura::OnOverviewModeEnded() {
  FOR_EACH_OBSERVER(WmOverviewModeObserver, overview_mode_observers_,
                    OnOverviewModeEnded());
}

}  // namespace ash
