// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"

#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/display.h"
#include "ui/views/corewm/window_util.h"

namespace ash {
namespace wm {

// static
bool WindowState::IsMaximizedOrFullscreenState(ui::WindowShowState show_state) {
  return show_state == ui::SHOW_STATE_FULLSCREEN ||
      show_state == ui::SHOW_STATE_MAXIMIZED;
}

WindowState::WindowState(aura::Window* window)
    : window_(window),
      tracked_by_workspace_(true),
      window_position_managed_(false),
      bounds_changed_by_user_(false),
      panel_attached_(true),
      continue_drag_after_reparent_(false),
      ignored_by_shelf_(false),
      always_restores_to_restore_bounds_(false) {
}

WindowState::~WindowState() {
}

ui::WindowShowState WindowState::GetShowState() const {
  return window_->GetProperty(aura::client::kShowStateKey);
}

bool WindowState::IsMinimized() const {
  return GetShowState() == ui::SHOW_STATE_MINIMIZED;
}

bool WindowState::IsMaximized() const {
  return GetShowState() == ui::SHOW_STATE_MAXIMIZED;
}

bool WindowState::IsFullscreen() const {
  return GetShowState() == ui::SHOW_STATE_FULLSCREEN;
}

bool WindowState::IsMaximizedOrFullscreen() const {
  return IsMaximizedOrFullscreenState(GetShowState());
}

bool WindowState::IsNormalShowState() const {
  ui::WindowShowState state = window_->GetProperty(aura::client::kShowStateKey);
  return state == ui::SHOW_STATE_NORMAL || state == ui::SHOW_STATE_DEFAULT;
}

bool WindowState::IsActive() const {
  return IsActiveWindow(window_);
}

bool WindowState::CanMaximize() const {
  return window_->GetProperty(aura::client::kCanMaximizeKey);
}

bool WindowState::CanMinimize() const {
  internal::RootWindowController* controller =
      internal::RootWindowController::ForWindow(window_);
  if (!controller)
    return false;
  aura::Window* lockscreen = controller->GetContainer(
      internal::kShellWindowId_LockScreenContainersContainer);
  if (lockscreen->Contains(window_))
    return false;

  return true;
}

bool WindowState::CanResize() const {
  return window_->GetProperty(aura::client::kCanResizeKey);
}

bool WindowState::CanActivate() const {
  return views::corewm::CanActivateWindow(window_);
}

bool WindowState::CanSnap() const {
  if (!CanResize())
    return false;
  if (window_->type() == aura::client::WINDOW_TYPE_PANEL)
    return false;
  // If a window has a maximum size defined, snapping may make it too big.
  return window_->delegate() ? window_->delegate()->GetMaximumSize().IsEmpty() :
                              true;
}

bool WindowState::HasRestoreBounds() const {
  return window_->GetProperty(aura::client::kRestoreBoundsKey) != NULL;
}

void WindowState::Maximize() {
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
}

void WindowState::Minimize() {
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
}

void WindowState::Unminimize() {
  window_->SetProperty(
      aura::client::kShowStateKey,
      window_->GetProperty(aura::client::kRestoreShowStateKey));
  window_->ClearProperty(aura::client::kRestoreShowStateKey);
}

void WindowState::Activate() {
  ActivateWindow(window_);
}

void WindowState::Deactivate() {
  DeactivateWindow(window_);
}

void WindowState::Restore() {
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
}

void WindowState::ToggleMaximized() {
  if (IsMaximized())
    Restore();
  else if (CanMaximize())
    Maximize();
}

void WindowState::SetBoundsInScreen(
    const gfx::Rect& bounds_in_screen) {
  gfx::Rect bounds_in_parent =
      ScreenAsh::ConvertRectFromScreen(window_->parent(),
                                       bounds_in_screen);
  window_->SetBounds(bounds_in_parent);
}

void WindowState::SaveCurrentBoundsForRestore() {
  gfx::Rect bounds_in_screen =
      ScreenAsh::ConvertRectToScreen(window_->parent(),
                                     window_->bounds());
  SetRestoreBoundsInScreen(bounds_in_screen);
}

gfx::Rect WindowState::GetRestoreBoundsInScreen() const {
  return *window_->GetProperty(aura::client::kRestoreBoundsKey);
}

gfx::Rect WindowState::GetRestoreBoundsInParent() const {
  return ScreenAsh::ConvertRectFromScreen(window_->parent(),
                                          GetRestoreBoundsInScreen());
}

void WindowState::SetRestoreBoundsInScreen(const gfx::Rect& bounds) {
  window_->SetProperty(aura::client::kRestoreBoundsKey, new gfx::Rect(bounds));
}

void WindowState::SetRestoreBoundsInParent(const gfx::Rect& bounds) {
  SetRestoreBoundsInScreen(
      ScreenAsh::ConvertRectToScreen(window_->parent(), bounds));
}

void WindowState::ClearRestoreBounds() {
  window_->ClearProperty(aura::client::kRestoreBoundsKey);
}

void WindowState::SetPreAutoManageWindowBounds(
    const gfx::Rect& bounds) {
  pre_auto_manage_window_bounds_.reset(new gfx::Rect(bounds));
}

void WindowState::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void WindowState::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void WindowState::SetTrackedByWorkspace(bool tracked_by_workspace) {
  if (tracked_by_workspace_ == tracked_by_workspace)
    return;
  bool old = tracked_by_workspace_;
  tracked_by_workspace_ = tracked_by_workspace;
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnTrackedByWorkspaceChanged(window_, old));
}

WindowState* GetActiveWindowState() {
  aura::Window* active = GetActiveWindow();
  return active ? GetWindowState(active) : NULL;
}

WindowState* GetWindowState(aura::Window* window) {
  if (!window)
    return NULL;
  WindowState* settings = window->GetProperty(internal::kWindowStateKey);
  if(!settings) {
    settings = new WindowState(window);
    window->SetProperty(internal::kWindowStateKey, settings);
  }
  return settings;
}

const WindowState* GetWindowState(const aura::Window* window) {
  return GetWindowState(const_cast<aura::Window*>(window));
}

}  // namespace wm
}  // namespace ash
