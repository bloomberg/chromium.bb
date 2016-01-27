// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"

#include <utility>

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/default_state.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace wm {

namespace {

// A tentative class to set the bounds on the window.
// TODO(oshima): Once all logic is cleaned up, move this to the real layout
// manager with proper friendship.
class BoundsSetter : public aura::LayoutManager {
 public:
  BoundsSetter() {}
  ~BoundsSetter() override {}

  // aura::LayoutManager overrides:
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override {}
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}

  void SetBounds(aura::Window* window, const gfx::Rect& bounds) {
    SetChildBoundsDirect(window, bounds);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BoundsSetter);
};

WMEventType WMEventTypeFromShowState(ui::WindowShowState requested_show_state) {
  switch (requested_show_state) {
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_NORMAL:
      return WM_EVENT_NORMAL;
    case ui::SHOW_STATE_MINIMIZED:
      return WM_EVENT_MINIMIZE;
    case ui::SHOW_STATE_MAXIMIZED:
      return WM_EVENT_MAXIMIZE;
    case ui::SHOW_STATE_FULLSCREEN:
      return WM_EVENT_FULLSCREEN;
    case ui::SHOW_STATE_INACTIVE:
      return WM_EVENT_SHOW_INACTIVE;
    case ui::SHOW_STATE_DOCKED:
      return WM_EVENT_DOCK;
    case ui::SHOW_STATE_END:
      NOTREACHED() << "No WMEvent defined for the show state:"
                   << requested_show_state;
  }
  return WM_EVENT_NORMAL;
}

}  // namespace

WindowState::~WindowState() {
  // WindowState is registered as an owned property of |window_|, and window
  // unregisters all of its observers in its d'tor before destroying its
  // properties. As a result, window_->RemoveObserver() doesn't need to (and
  // shouldn't) be called here.
}

bool WindowState::HasDelegate() const {
  return !!delegate_;
}

void WindowState::SetDelegate(scoped_ptr<WindowStateDelegate> delegate) {
  DCHECK(!delegate_.get());
  delegate_ = std::move(delegate);
}

WindowStateType WindowState::GetStateType() const {
  return current_state_->GetType();
}

bool WindowState::IsMinimized() const {
  return GetStateType() == WINDOW_STATE_TYPE_MINIMIZED ||
         GetStateType() == WINDOW_STATE_TYPE_DOCKED_MINIMIZED;
}

bool WindowState::IsMaximized() const {
  return GetStateType() == WINDOW_STATE_TYPE_MAXIMIZED;
}

bool WindowState::IsFullscreen() const {
  return GetStateType() == WINDOW_STATE_TYPE_FULLSCREEN;
}

bool WindowState::IsMaximizedOrFullscreen() const {
  return GetStateType() == WINDOW_STATE_TYPE_FULLSCREEN ||
      GetStateType() == WINDOW_STATE_TYPE_MAXIMIZED;
}

bool WindowState::IsSnapped() const {
  return GetStateType() == WINDOW_STATE_TYPE_LEFT_SNAPPED ||
      GetStateType() == WINDOW_STATE_TYPE_RIGHT_SNAPPED;
}

bool WindowState::IsNormalStateType() const {
  return GetStateType() == WINDOW_STATE_TYPE_NORMAL ||
      GetStateType() == WINDOW_STATE_TYPE_DEFAULT;
}

bool WindowState::IsNormalOrSnapped() const {
  return IsNormalStateType() || IsSnapped();
}

bool WindowState::IsActive() const {
  return IsActiveWindow(window_);
}

bool WindowState::IsDocked() const {
  return GetStateType() == WINDOW_STATE_TYPE_DOCKED ||
         GetStateType() == WINDOW_STATE_TYPE_DOCKED_MINIMIZED;
}

bool WindowState::IsUserPositionable() const {
  return (window()->type() == ui::wm::WINDOW_TYPE_NORMAL ||
          window()->type() == ui::wm::WINDOW_TYPE_PANEL);
}

bool WindowState::CanMaximize() const {
  // Window must have the kCanMaximizeKey and have no maximum width or height.
  if (!window()->GetProperty(aura::client::kCanMaximizeKey))
    return false;

  if (!window()->delegate())
    return true;

  gfx::Size max_size = window_->delegate()->GetMaximumSize();
  return !max_size.width() && !max_size.height();
}

bool WindowState::CanMinimize() const {
  return window()->GetProperty(aura::client::kCanMinimizeKey);
}

bool WindowState::CanResize() const {
  return window_->GetProperty(aura::client::kCanResizeKey);
}

bool WindowState::CanActivate() const {
  return ::wm::CanActivateWindow(window_);
}

bool WindowState::CanSnap() const {
  if (!CanResize() || window_->type() == ui::wm::WINDOW_TYPE_PANEL ||
      ::wm::GetTransientParent(window_))
    return false;
  // If a window cannot be maximized, assume it cannot snap either.
  // TODO(oshima): We should probably snap if the maximum size is greater than
  // the snapped size.
  return CanMaximize();
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
  if (!IsNormalStateType()) {
    const WMEvent event(WM_EVENT_NORMAL);
    OnWMEvent(&event);
  }
}

void WindowState::DisableAlwaysOnTop(aura::Window* window_on_top) {
  DCHECK(window_on_top);
  if (GetAlwaysOnTop()) {
    // |window_| is hidden first to avoid canceling fullscreen mode when it is
    // no longer always on top and gets added to default container. This avoids
    // sending redundant OnFullscreenStateChanged to the layout manager. The
    // |window_| visibility is restored after it no longer obscures the
    // |window_on_top|.
    bool visible = window_->IsVisible();
    if (visible)
      window_->Hide();
    window_->SetProperty(aura::client::kAlwaysOnTopKey, false);
    // Technically it is possible that a |window_| could make itself
    // always_on_top really quickly. This is probably not a realistic case but
    // check if the two windows are in the same container just in case.
    if (window_on_top->parent() == window_->parent())
      window_->parent()->StackChildAbove(window_on_top, window_);
    if (visible)
      window_->Show();
    cached_always_on_top_ = true;
  }
}

void WindowState::RestoreAlwaysOnTop() {
  if (delegate() && delegate()->RestoreAlwaysOnTop(this))
    return;
  if (cached_always_on_top_) {
    cached_always_on_top_ = false;
    window_->SetProperty(aura::client::kAlwaysOnTopKey, true);
  }
}

void WindowState::OnWMEvent(const WMEvent* event) {
  current_state_->OnWMEvent(this, event);
}

void WindowState::SaveCurrentBoundsForRestore() {
  gfx::Rect bounds_in_screen =
      ScreenUtil::ConvertRectToScreen(window_->parent(),
                                      window_->bounds());
  SetRestoreBoundsInScreen(bounds_in_screen);
}

gfx::Rect WindowState::GetRestoreBoundsInScreen() const {
  return *window_->GetProperty(aura::client::kRestoreBoundsKey);
}

gfx::Rect WindowState::GetRestoreBoundsInParent() const {
  return ScreenUtil::ConvertRectFromScreen(window_->parent(),
                                          GetRestoreBoundsInScreen());
}

void WindowState::SetRestoreBoundsInScreen(const gfx::Rect& bounds) {
  window_->SetProperty(aura::client::kRestoreBoundsKey, new gfx::Rect(bounds));
}

void WindowState::SetRestoreBoundsInParent(const gfx::Rect& bounds) {
  SetRestoreBoundsInScreen(
      ScreenUtil::ConvertRectToScreen(window_->parent(), bounds));
}

void WindowState::ClearRestoreBounds() {
  window_->ClearProperty(aura::client::kRestoreBoundsKey);
}

scoped_ptr<WindowState::State> WindowState::SetStateObject(
    scoped_ptr<WindowState::State> new_state) {
  current_state_->DetachState(this);
  scoped_ptr<WindowState::State> old_object = std::move(current_state_);
  current_state_ = std::move(new_state);
  current_state_->AttachState(this, old_object.get());
  return old_object;
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

void WindowState::set_bounds_changed_by_user(bool bounds_changed_by_user) {
  bounds_changed_by_user_ = bounds_changed_by_user;
  if (bounds_changed_by_user)
    pre_auto_manage_window_bounds_.reset();
}

void WindowState::CreateDragDetails(aura::Window* window,
                                    const gfx::Point& point_in_parent,
                                    int window_component,
                                    aura::client::WindowMoveSource source) {
  drag_details_.reset(
      new DragDetails(window, point_in_parent, window_component, source));
}

void WindowState::DeleteDragDetails() {
  drag_details_.reset();
}

void WindowState::SetAndClearRestoreBounds() {
  DCHECK(HasRestoreBounds());
  SetBoundsInScreen(GetRestoreBoundsInScreen());
  ClearRestoreBounds();
}

void WindowState::OnWindowPropertyChanged(aura::Window* window,
                                          const void* key,
                                          intptr_t old) {
  DCHECK_EQ(window, window_);
  if (key == aura::client::kShowStateKey && !ignore_property_change_) {
    WMEvent event(WMEventTypeFromShowState(GetShowState()));
    OnWMEvent(&event);
  }
}

WindowState::WindowState(aura::Window* window)
    : window_(window),
      window_position_managed_(false),
      bounds_changed_by_user_(false),
      panel_attached_(true),
      ignored_by_shelf_(false),
      can_consume_system_keys_(false),
      top_row_keys_are_function_keys_(false),
      unminimize_to_restore_bounds_(false),
      in_immersive_fullscreen_(false),
      hide_shelf_when_fullscreen_(true),
      minimum_visibility_(false),
      can_be_dragged_(true),
      cached_always_on_top_(false),
      ignore_property_change_(false),
      current_state_(new DefaultState(ToWindowStateType(GetShowState()))) {
  window_->AddObserver(this);
}

bool WindowState::GetAlwaysOnTop() const {
  return window_->GetProperty(aura::client::kAlwaysOnTopKey);
}

ui::WindowShowState WindowState::GetShowState() const {
  return window_->GetProperty(aura::client::kShowStateKey);
}

void WindowState::SetBoundsInScreen(
    const gfx::Rect& bounds_in_screen) {
  gfx::Rect bounds_in_parent =
      ScreenUtil::ConvertRectFromScreen(window_->parent(),
                                       bounds_in_screen);
  window_->SetBounds(bounds_in_parent);
}

void WindowState::AdjustSnappedBounds(gfx::Rect* bounds) {
  if (is_dragged() || !IsSnapped())
    return;
  gfx::Rect maximized_bounds = ScreenUtil::GetMaximizedWindowBoundsInParent(
      window_);
  if (GetStateType() == WINDOW_STATE_TYPE_LEFT_SNAPPED)
    bounds->set_x(maximized_bounds.x());
  else if (GetStateType() == WINDOW_STATE_TYPE_RIGHT_SNAPPED)
    bounds->set_x(maximized_bounds.right() - bounds->width());
  bounds->set_y(maximized_bounds.y());
  bounds->set_height(maximized_bounds.height());
}

void WindowState::UpdateWindowShowStateFromStateType() {
  ui::WindowShowState new_window_state =
      ToWindowShowState(current_state_->GetType());
  if (new_window_state != GetShowState()) {
    base::AutoReset<bool> resetter(&ignore_property_change_, true);
    window_->SetProperty(aura::client::kShowStateKey, new_window_state);
  }
}

void WindowState::NotifyPreStateTypeChange(
    WindowStateType old_window_state_type) {
  FOR_EACH_OBSERVER(WindowStateObserver, observer_list_,
                    OnPreWindowStateTypeChange(this, old_window_state_type));
}

void WindowState::NotifyPostStateTypeChange(
    WindowStateType old_window_state_type) {
  FOR_EACH_OBSERVER(WindowStateObserver, observer_list_,
                    OnPostWindowStateTypeChange(this, old_window_state_type));
}

void WindowState::SetBoundsDirect(const gfx::Rect& bounds) {
  gfx::Rect actual_new_bounds(bounds);
  // Ensure we don't go smaller than our minimum bounds in "normal" window
  // modes
  if (window_->delegate() && !IsMaximized() && !IsFullscreen()) {
    // Get the minimum usable size of the minimum size and the screen size.
    gfx::Size min_size = window_->delegate()->GetMinimumSize();
    min_size.SetToMin(gfx::Screen::GetScreen()
                          ->GetDisplayNearestWindow(window_)
                          .work_area()
                          .size());

    actual_new_bounds.set_width(
        std::max(min_size.width(), actual_new_bounds.width()));
    actual_new_bounds.set_height(
        std::max(min_size.height(), actual_new_bounds.height()));
  }
  BoundsSetter().SetBounds(window_, actual_new_bounds);
  SnapWindowToPixelBoundary(window_);
}

void WindowState::SetBoundsConstrained(const gfx::Rect& bounds) {
  gfx::Rect work_area_in_parent =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(window_);
  gfx::Rect child_bounds(bounds);
  AdjustBoundsSmallerThan(work_area_in_parent.size(), &child_bounds);
  SetBoundsDirect(child_bounds);
}

void WindowState::SetBoundsDirectAnimated(const gfx::Rect& bounds) {
  const int kBoundsChangeSlideDurationMs = 120;

  ui::Layer* layer = window_->layer();
  ui::ScopedLayerAnimationSettings slide_settings(layer->GetAnimator());
  slide_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  slide_settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kBoundsChangeSlideDurationMs));
  SetBoundsDirect(bounds);
}

void WindowState::SetBoundsDirectCrossFade(const gfx::Rect& new_bounds) {
  // Some test results in invoking CrossFadeToBounds when window is not visible.
  // No animation is necessary in that case, thus just change the bounds and
  // quit.
  if (!window_->TargetVisibility()) {
    SetBoundsConstrained(new_bounds);
    return;
  }

  const gfx::Rect old_bounds = window_->bounds();

  // Create fresh layers for the window and all its children to paint into.
  // Takes ownership of the old layer and all its children, which will be
  // cleaned up after the animation completes.
  // Specify |set_bounds| to true here to keep the old bounds in the child
  // windows of |window|.
  scoped_ptr<ui::LayerTreeOwner> old_layer_owner =
      ::wm::RecreateLayers(window_);
  ui::Layer* old_layer = old_layer_owner->root();
  DCHECK(old_layer);
  ui::Layer* new_layer = window_->layer();

  // Resize the window to the new size, which will force a layout and paint.
  SetBoundsDirect(new_bounds);

  // Ensure the higher-resolution layer is on top.
  bool old_on_top = (old_bounds.width() > new_bounds.width());
  if (old_on_top)
    old_layer->parent()->StackBelow(new_layer, old_layer);
  else
    old_layer->parent()->StackAbove(new_layer, old_layer);

  CrossFadeAnimation(window_, std::move(old_layer_owner), gfx::Tween::EASE_OUT);
}

WindowState* GetActiveWindowState() {
  aura::Window* active = GetActiveWindow();
  return active ? GetWindowState(active) : NULL;
}

WindowState* GetWindowState(aura::Window* window) {
  if (!window)
    return NULL;
  WindowState* settings = window->GetProperty(kWindowStateKey);
  if(!settings) {
    settings = new WindowState(window);
    window->SetProperty(kWindowStateKey, settings);
  }
  return settings;
}

const WindowState* GetWindowState(const aura::Window* window) {
  return GetWindowState(const_cast<aura::Window*>(window));
}

}  // namespace wm
}  // namespace ash
