// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"

#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_types.h"
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
      can_consume_system_keys_(false),
      top_row_keys_are_function_keys_(false),
      window_resizer_(NULL),
      always_restores_to_restore_bounds_(false),
      hide_shelf_when_fullscreen_(true),
      animate_to_fullscreen_(true),
      minimum_visibility_(false),
      window_show_type_(ToWindowShowType(GetShowState())) {
  window_->AddObserver(this);
}

WindowState::~WindowState() {
}

void WindowState::SetDelegate(scoped_ptr<WindowStateDelegate> delegate) {
  DCHECK(!delegate_.get());
  delegate_ = delegate.Pass();
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

bool WindowState::IsDocked() const {
  return window_->parent() &&
      window_->parent()->id() == internal::kShellWindowId_DockedContainer;
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
  if (!CanResize() ||
      window_->type() == aura::client::WINDOW_TYPE_PANEL ||
      window_->transient_parent())
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

void WindowState::SnapLeft(const gfx::Rect& bounds) {
  SnapWindow(SHOW_TYPE_LEFT_SNAPPED, bounds);
}

void WindowState::SnapRight(const gfx::Rect& bounds) {
  SnapWindow(SHOW_TYPE_LEFT_SNAPPED, bounds);
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

void WindowState::ToggleFullscreen() {
  // Window which cannot be maximized should not be fullscreened.
  // It can, however, be restored if it was fullscreened.
  bool is_fullscreen = IsFullscreen();
  if (!is_fullscreen && !CanMaximize())
    return;
  if (delegate_ && delegate_->ToggleFullscreen(this))
    return;
  if (is_fullscreen) {
    Restore();
  } else {
    window_->SetProperty(aura::client::kShowStateKey,
                         ui::SHOW_STATE_FULLSCREEN);
  }
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

void WindowState::AddObserver(WindowStateObserver* observer) {
  observer_list_.AddObserver(observer);
}

void WindowState::RemoveObserver(WindowStateObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void WindowState::SetTrackedByWorkspace(bool tracked_by_workspace) {
  if (tracked_by_workspace_ == tracked_by_workspace)
    return;
  bool old = tracked_by_workspace_;
  tracked_by_workspace_ = tracked_by_workspace;
  FOR_EACH_OBSERVER(WindowStateObserver, observer_list_,
                    OnTrackedByWorkspaceChanged(this, old));
}

void WindowState::OnWindowPropertyChanged(aura::Window* window,
                                          const void* key,
                                          intptr_t old) {
  DCHECK_EQ(window, window_);
  if (key == aura::client::kShowStateKey) {
    window_show_type_ = ToWindowShowType(GetShowState());
    ui::WindowShowState old_state = static_cast<ui::WindowShowState>(old);
    // TODO(oshima): Notify only when the state has changed.
    // Doing so break a few tests now.
    FOR_EACH_OBSERVER(
        WindowStateObserver, observer_list_,
        OnWindowShowTypeChanged(this, ToWindowShowType(old_state)));
  }
}

void WindowState::OnWindowDestroying(aura::Window* window) {
  window_->RemoveObserver(this);
}

void WindowState::SnapWindow(WindowShowType left_or_right,
                             const gfx::Rect& bounds) {
  if (IsMaximizedOrFullscreen()) {
    // Before we can set the bounds we need to restore the window.
    // Restoring the window will set the window to its restored bounds.
    // To avoid an unnecessary bounds changes (which may have side effects)
    // we set the restore bounds to the bounds we want, restore the window,
    // then reset the restore bounds. This way no unnecessary bounds
    // changes occurs and the original restore bounds is remembered.
    gfx::Rect restore_bounds_in_screen =
        GetRestoreBoundsInScreen();
    SetRestoreBoundsInParent(bounds);
    Restore();
    SetRestoreBoundsInScreen(restore_bounds_in_screen);
  } else {
    window_->SetBounds(bounds);
  }
  DCHECK(left_or_right == SHOW_TYPE_LEFT_SNAPPED ||
         left_or_right == SHOW_TYPE_RIGHT_SNAPPED);
  window_show_type_ = left_or_right;
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
