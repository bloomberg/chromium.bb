// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_shell_aura.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_observer.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm_activation_observer.h"
#include "ash/common/wm_display_observer.h"
#include "ash/display/display_manager.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/maximize_mode/maximize_mode_event_handler_aura.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_util.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/client/focus_client.h"
#include "ui/wm/public/activation_client.h"

#if defined(OS_CHROMEOS)
#include "ash/virtual_keyboard_controller.h"
#endif

namespace ash {

WmShellAura::WmShellAura() {
  WmShell::Set(this);
}

WmShellAura::~WmShellAura() {
  WmShell::Set(nullptr);
}

void WmShellAura::PrepareForShutdown() {
  if (added_activation_observer_)
    Shell::GetInstance()->activation_client()->RemoveObserver(this);

  if (added_display_observer_)
    Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);
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

const DisplayInfo& WmShellAura::GetDisplayInfo(int64_t display_id) const {
  return Shell::GetInstance()->display_manager()->GetDisplayInfo(display_id);
}

bool WmShellAura::IsForceMaximizeOnFirstRun() {
  return Shell::GetInstance()->delegate()->IsForceMaximizeOnFirstRun();
}

bool WmShellAura::IsPinned() {
  return Shell::GetInstance()->screen_pinning_controller()->IsPinned();
}

void WmShellAura::SetPinnedWindow(WmWindow* window) {
  return Shell::GetInstance()->screen_pinning_controller()->SetPinnedWindow(
      window);
}

bool WmShellAura::CanShowWindowForUser(WmWindow* window) {
  return Shell::GetInstance()->delegate()->CanShowWindowForUser(window);
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

void WmShellAura::RecordUserMetricsAction(UserMetricsAction action) {
  return Shell::GetInstance()->metrics()->RecordUserMetricsAction(action);
}

std::unique_ptr<WindowResizer> WmShellAura::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  return base::WrapUnique(
      DragWindowResizer::Create(next_window_resizer.release(), window_state));
}

std::unique_ptr<wm::MaximizeModeEventHandler>
WmShellAura::CreateMaximizeModeEventHandler() {
  return base::WrapUnique(new wm::MaximizeModeEventHandlerAura);
}

void WmShellAura::OnOverviewModeStarting() {
  FOR_EACH_OBSERVER(ShellObserver, *shell_observers(),
                    OnOverviewModeStarting());
}

void WmShellAura::OnOverviewModeEnded() {
  FOR_EACH_OBSERVER(ShellObserver, *shell_observers(), OnOverviewModeEnded());
}

AccessibilityDelegate* WmShellAura::GetAccessibilityDelegate() {
  return Shell::GetInstance()->accessibility_delegate();
}

SessionStateDelegate* WmShellAura::GetSessionStateDelegate() {
  return Shell::GetInstance()->session_state_delegate();
}

void WmShellAura::AddActivationObserver(WmActivationObserver* observer) {
  if (!added_activation_observer_) {
    added_activation_observer_ = true;
    Shell::GetInstance()->activation_client()->AddObserver(this);
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

void WmShellAura::AddPointerWatcher(views::PointerWatcher* watcher) {
  Shell::GetInstance()->AddPointerWatcher(watcher);
}

void WmShellAura::RemovePointerWatcher(views::PointerWatcher* watcher) {
  Shell::GetInstance()->RemovePointerWatcher(watcher);
}

#if defined(OS_CHROMEOS)
void WmShellAura::ToggleIgnoreExternalKeyboard() {
  Shell::GetInstance()
      ->virtual_keyboard_controller()
      ->ToggleIgnoreExternalKeyboard();
}
#endif

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

}  // namespace ash
