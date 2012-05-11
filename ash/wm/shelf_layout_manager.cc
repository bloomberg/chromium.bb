// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/shelf_layout_manager.h"

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "base/auto_reset.h"
#include "base/i18n/rtl.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

// Delay before showing the launcher. This is after the mouse stops moving.
const int kAutoHideDelayMS = 200;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

}  // namespace

// static
const int ShelfLayoutManager::kWorkspaceAreaBottomInset = 2;

// static
const int ShelfLayoutManager::kAutoHideSize = 2;

// Notifies ShelfLayoutManager any time the mouse moves.
class ShelfLayoutManager::AutoHideEventFilter : public aura::EventFilter {
 public:
  explicit AutoHideEventFilter(ShelfLayoutManager* shelf);
  virtual ~AutoHideEventFilter();

  // Returns true if the last mouse event was a mouse drag.
  bool in_mouse_drag() const { return in_mouse_drag_; }

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

 private:
  ShelfLayoutManager* shelf_;
  bool in_mouse_drag_;

  DISALLOW_COPY_AND_ASSIGN(AutoHideEventFilter);
};

ShelfLayoutManager::AutoHideEventFilter::AutoHideEventFilter(
    ShelfLayoutManager* shelf)
    : shelf_(shelf),
      in_mouse_drag_(false) {
  Shell::GetInstance()->AddRootWindowEventFilter(this);
}

ShelfLayoutManager::AutoHideEventFilter::~AutoHideEventFilter() {
  Shell::GetInstance()->RemoveRootWindowEventFilter(this);
}

bool ShelfLayoutManager::AutoHideEventFilter::PreHandleKeyEvent(
    aura::Window* target,
    aura::KeyEvent* event) {
  return false;  // Always let the event propagate.
}

bool ShelfLayoutManager::AutoHideEventFilter::PreHandleMouseEvent(
    aura::Window* target,
    aura::MouseEvent* event) {
  // This also checks IsShelfWindow() to make sure we don't attempt to hide the
  // shelf if the mouse down occurs on the shelf.
  in_mouse_drag_ = (event->type() == ui::ET_MOUSE_DRAGGED ||
                    (in_mouse_drag_ && event->type() != ui::ET_MOUSE_RELEASED &&
                     event->type() != ui::ET_MOUSE_CAPTURE_CHANGED)) &&
      !shelf_->IsShelfWindow(target);
  if (event->type() == ui::ET_MOUSE_MOVED)
    shelf_->UpdateAutoHideState();
  return false;  // Not handled.
}

ui::TouchStatus ShelfLayoutManager::AutoHideEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;  // Not handled.
}

ui::GestureStatus
ShelfLayoutManager::AutoHideEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;  // Not handled.
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, public:

ShelfLayoutManager::ShelfLayoutManager(views::Widget* status)
    : root_window_(Shell::GetInstance()->GetRootWindow()),
      in_layout_(false),
      auto_hide_behavior_(SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT),
      alignment_(SHELF_ALIGNMENT_BOTTOM),
      launcher_(NULL),
      status_(status),
      workspace_manager_(NULL),
      window_overlaps_shelf_(false) {
  Shell::GetInstance()->AddShellObserver(this);
  root_window_->AddObserver(this);
}

ShelfLayoutManager::~ShelfLayoutManager() {
  root_window_->RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
}

void ShelfLayoutManager::SetAutoHideBehavior(ShelfAutoHideBehavior behavior) {
  if (auto_hide_behavior_ == behavior)
    return;
  auto_hide_behavior_ = behavior;
  UpdateVisibilityState();
}

bool ShelfLayoutManager::IsVisible() const {
  return status_->IsVisible() && (state_.visibility_state == VISIBLE ||
      (state_.visibility_state == AUTO_HIDE &&
       state_.auto_hide_state == AUTO_HIDE_SHOWN));
}

gfx::Rect ShelfLayoutManager::GetMaximizedWindowBounds(
    aura::Window* window) {
  // TODO: needs to be multi-mon aware.
  gfx::Rect bounds(gfx::Screen::GetMonitorNearestWindow(window).bounds());
  if (auto_hide_behavior_ == SHELF_AUTO_HIDE_BEHAVIOR_DEFAULT ||
      auto_hide_behavior_ == SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS) {
    AdjustBoundsBasedOnAlignment(kAutoHideSize, &bounds);
    return bounds;
  }
  // SHELF_AUTO_HIDE_BEHAVIOR_NEVER maximized windows don't get any taller.
  return GetUnmaximizedWorkAreaBounds(window);
}

gfx::Rect ShelfLayoutManager::GetUnmaximizedWorkAreaBounds(
    aura::Window* window) {
  // TODO: needs to be multi-mon aware.
  gfx::Rect bounds(gfx::Screen::GetMonitorNearestWindow(window).bounds());
  int size;
  if (auto_hide_behavior_ == SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS) {
    size = kAutoHideSize;
  } else {
    int width, height;
    GetShelfSize(&width, &height);
    size = std::max(width, height);
  }
  AdjustBoundsBasedOnAlignment(size, &bounds);
  return bounds;
}

void ShelfLayoutManager::SetLauncher(Launcher* launcher) {
  if (launcher == launcher_)
    return;

  launcher_ = launcher;
  LayoutShelf();
}

void ShelfLayoutManager::SetAlignment(ShelfAlignment alignment) {
  if (alignment_ == alignment)
    return;

  alignment_ = alignment;
  if (launcher_)
    launcher_->SetAlignment(alignment);
  LayoutShelf();
}

gfx::Rect ShelfLayoutManager::GetIdealBounds() {
  // TODO: this is wrong. Figure out what monitor shelf is on and everything
  // should be based on it.
  gfx::Rect bounds(
      gfx::Screen::GetMonitorNearestWindow(status_->GetNativeView()).bounds());
  int width = 0, height = 0;
  GetShelfSize(&width, &height);
  switch (alignment_) {
    case SHELF_ALIGNMENT_BOTTOM:
      return gfx::Rect(bounds.x(), bounds.bottom() - height,
                       bounds.width(), height);
    case SHELF_ALIGNMENT_LEFT:
      return gfx::Rect(bounds.x(), bounds.y(), width, bounds.height());
    case SHELF_ALIGNMENT_RIGHT:
      return gfx::Rect(bounds.right() - width, bounds.bottom() - height, width,
                       height);
  }
  NOTREACHED();
  return gfx::Rect();
}

void ShelfLayoutManager::LayoutShelf() {
  AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
  StopAnimating();
  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  if (launcher_widget()) {
    GetLayer(launcher_widget())->SetOpacity(target_bounds.opacity);

    launcher_widget()->SetBounds(target_bounds.launcher_bounds);
    launcher_->SetStatusSize(target_bounds.status_bounds.size());
  }
  GetLayer(status_)->SetOpacity(target_bounds.opacity);
  status_->SetBounds(target_bounds.status_bounds);
  Shell::GetInstance()->SetMonitorWorkAreaInsets(
      Shell::GetRootWindow(),
      target_bounds.work_area_insets);
  UpdateHitTestBounds();
}

void ShelfLayoutManager::UpdateVisibilityState() {
  ShellDelegate* delegate = Shell::GetInstance()->delegate();
  if (delegate && delegate->IsScreenLocked()) {
    SetState(VISIBLE);
  } else {
    WorkspaceManager::WindowState window_state(
        workspace_manager_->GetWindowState());
    switch (window_state) {
      case WorkspaceManager::WINDOW_STATE_FULL_SCREEN:
        SetState(HIDDEN);
        break;

      case WorkspaceManager::WINDOW_STATE_MAXIMIZED:
        SetState(auto_hide_behavior_ != SHELF_AUTO_HIDE_BEHAVIOR_NEVER ?
                 AUTO_HIDE : VISIBLE);
        break;

      case WorkspaceManager::WINDOW_STATE_WINDOW_OVERLAPS_SHELF:
      case WorkspaceManager::WINDOW_STATE_DEFAULT:
        SetState(auto_hide_behavior_ == SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS ?
                 AUTO_HIDE : VISIBLE);
        SetWindowOverlapsShelf(window_state ==
            WorkspaceManager::WINDOW_STATE_WINDOW_OVERLAPS_SHELF);
    }
  }
}

void ShelfLayoutManager::UpdateAutoHideState() {
  AutoHideState auto_hide_state =
      CalculateAutoHideState(state_.visibility_state);
  if (auto_hide_state != state_.auto_hide_state) {
    if (auto_hide_state == AUTO_HIDE_HIDDEN) {
      // Hides happen immediately.
      SetState(state_.visibility_state);
    } else {
      auto_hide_timer_.Stop();
      auto_hide_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kAutoHideDelayMS),
          this, &ShelfLayoutManager::UpdateAutoHideStateNow);
    }
  } else {
    auto_hide_timer_.Stop();
  }
}

void ShelfLayoutManager::SetWindowOverlapsShelf(bool value) {
  window_overlaps_shelf_ = value;
  UpdateShelfBackground(internal::BackgroundAnimator::CHANGE_ANIMATE);
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, aura::LayoutManager implementation:

void ShelfLayoutManager::OnWindowResized() {
  LayoutShelf();
}

void ShelfLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
}

void ShelfLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
}

void ShelfLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
}

void ShelfLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                        bool visible) {
}

void ShelfLayoutManager::SetChildBounds(aura::Window* child,
                                        const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
  if (!in_layout_)
    LayoutShelf();
}

void ShelfLayoutManager::OnLockStateChanged(bool locked) {
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                 const void* key,
                                                 intptr_t old) {
  if (key == aura::client::kRootWindowActiveWindowKey)
    UpdateAutoHideStateNow();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, private:

void ShelfLayoutManager::SetState(VisibilityState visibility_state) {
  ShellDelegate* delegate = Shell::GetInstance()->delegate();
  State state;
  state.visibility_state = visibility_state;
  state.auto_hide_state = CalculateAutoHideState(visibility_state);
  state.is_screen_locked = delegate && delegate->IsScreenLocked();

  if (state_.Equals(state))
    return;  // Nothing changed.

  if (state.visibility_state == AUTO_HIDE) {
    // When state is AUTO_HIDE we need to track when the mouse is over the
    // launcher to unhide the shelf. AutoHideEventFilter does that for us.
    if (!event_filter_.get())
      event_filter_.reset(new AutoHideEventFilter(this));
  } else {
    event_filter_.reset(NULL);
  }

  auto_hide_timer_.Stop();

  // Animating the background when transitioning from auto-hide & hidden to
  // visibile is janking. Update the background immediately in this case.
  internal::BackgroundAnimator::ChangeType change_type =
      (state_.visibility_state == AUTO_HIDE &&
       state_.auto_hide_state == AUTO_HIDE_HIDDEN &&
       state.visibility_state == VISIBLE) ?
      internal::BackgroundAnimator::CHANGE_IMMEDIATE :
      internal::BackgroundAnimator::CHANGE_ANIMATE;
  StopAnimating();
  state_ = state;
  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  if (launcher_widget()) {
    ui::ScopedLayerAnimationSettings launcher_animation_setter(
        GetLayer(launcher_widget())->GetAnimator());
    launcher_animation_setter.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(130));
    launcher_animation_setter.SetTweenType(ui::Tween::EASE_OUT);
    GetLayer(launcher_widget())->SetBounds(target_bounds.launcher_bounds);
    GetLayer(launcher_widget())->SetOpacity(target_bounds.opacity);
  }
  ui::ScopedLayerAnimationSettings status_animation_setter(
      GetLayer(status_)->GetAnimator());
  status_animation_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(130));
  status_animation_setter.SetTweenType(ui::Tween::EASE_OUT);
  GetLayer(status_)->SetBounds(target_bounds.status_bounds);
  GetLayer(status_)->SetOpacity(target_bounds.opacity);
  Shell::GetInstance()->SetMonitorWorkAreaInsets(
      Shell::GetRootWindow(),
      target_bounds.work_area_insets);
  UpdateHitTestBounds();
  UpdateShelfBackground(change_type);
}

void ShelfLayoutManager::StopAnimating() {
  if (launcher_widget())
    GetLayer(launcher_widget())->GetAnimator()->StopAnimating();
  GetLayer(status_)->GetAnimator()->StopAnimating();
}

void ShelfLayoutManager::GetShelfSize(int* width, int* height) {
  *width = *height = 0;
  gfx::Rect status_bounds(status_->GetWindowScreenBounds());
  gfx::Size launcher_size = launcher_ ?
      launcher_widget()->GetContentsView()->GetPreferredSize() : gfx::Size();
  if (alignment_ == SHELF_ALIGNMENT_BOTTOM) {
    *height = std::max(launcher_size.height(), status_bounds.height());
  } else {
    // TODO: include status when supports alignment.
    *width = launcher_size.width();
  }
}

void ShelfLayoutManager::AdjustBoundsBasedOnAlignment(int inset,
                                                      gfx::Rect* bounds) const {
  switch (alignment_) {
    case SHELF_ALIGNMENT_BOTTOM:
      bounds->Inset(gfx::Insets(0, 0, inset, 0));
      break;
    case SHELF_ALIGNMENT_LEFT:
      bounds->Inset(gfx::Insets(0, inset, 0, 0));
      break;
    case SHELF_ALIGNMENT_RIGHT:
      bounds->Inset(gfx::Insets(0, 0, 0, inset));
      break;
  }
}

void ShelfLayoutManager::CalculateTargetBounds(
    const State& state,
    TargetBounds* target_bounds) {
  const gfx::Rect& available_bounds(
      status_->GetNativeView()->GetRootWindow()->bounds());
  gfx::Rect status_bounds(status_->GetWindowScreenBounds());
  gfx::Size launcher_size = launcher_ ?
      launcher_widget()->GetContentsView()->GetPreferredSize() : gfx::Size();
  int shelf_size = 0;
  int shelf_width = 0, shelf_height = 0;
  GetShelfSize(&shelf_width, &shelf_height);
  if (state.visibility_state == VISIBLE ||
      (state.visibility_state == AUTO_HIDE &&
       state.auto_hide_state == AUTO_HIDE_SHOWN)) {
    shelf_size = std::max(shelf_width, shelf_height);
  } else if (state.visibility_state == AUTO_HIDE &&
             state.auto_hide_state == AUTO_HIDE_HIDDEN) {
    shelf_size = kAutoHideSize;
  }
  if (alignment_ == SHELF_ALIGNMENT_BOTTOM) {
    int y = available_bounds.bottom();
    y -= shelf_size;
    // The status widget should extend to the bottom and right edges.
    target_bounds->status_bounds = gfx::Rect(
        base::i18n::IsRTL() ? available_bounds.x() :
        available_bounds.right() - status_bounds.width(),
        y + shelf_height - status_bounds.height(),
        status_bounds.width(), status_bounds.height());
    if (launcher_widget()) {
      target_bounds->launcher_bounds = gfx::Rect(
          available_bounds.x(),
          y + (shelf_height - launcher_size.height()) / 2,
          available_bounds.width(),
          launcher_size.height());
    }
    target_bounds->work_area_insets.Set(
        0, 0, GetWorkAreaSize(state, shelf_height), 0);
  } else {
    int x = (alignment_ == SHELF_ALIGNMENT_LEFT) ?
        available_bounds.x() + shelf_size - shelf_width :
        available_bounds.right() - shelf_size;
    target_bounds->status_bounds = gfx::Rect(
        x, available_bounds.bottom() - status_bounds.height(),
        shelf_width, status_bounds.height());
    if (launcher_widget()) {
      target_bounds->launcher_bounds = gfx::Rect(
          x,
          available_bounds.y(),
          launcher_size.width(),
          available_bounds.height());
    }
    if (alignment_ == SHELF_ALIGNMENT_LEFT) {
      target_bounds->work_area_insets.Set(
          0, GetWorkAreaSize(state, shelf_width), 0, 0);
    } else {
      target_bounds->work_area_insets.Set(
          0, 0, 0, GetWorkAreaSize(state, shelf_width));
    }
  }
  target_bounds->opacity =
      (state.visibility_state == VISIBLE ||
       state.visibility_state == AUTO_HIDE) ? 1.0f : 0.0f;
}

void ShelfLayoutManager::UpdateShelfBackground(
    BackgroundAnimator::ChangeType type) {
  bool launcher_paints = GetLauncherPaintsBackground();
  if (launcher_)
    launcher_->SetPaintsBackground(launcher_paints, type);
  // SystemTray normally draws a background, but we don't want it to draw a
  // background when the launcher does.
  if (Shell::GetInstance()->tray())
    Shell::GetInstance()->tray()->SetPaintsBackground(!launcher_paints, type);
}

bool ShelfLayoutManager::GetLauncherPaintsBackground() const {
  return (!state_.is_screen_locked && window_overlaps_shelf_) ||
      state_.visibility_state == AUTO_HIDE;
}

void ShelfLayoutManager::UpdateAutoHideStateNow() {
  SetState(state_.visibility_state);
}

ShelfLayoutManager::AutoHideState ShelfLayoutManager::CalculateAutoHideState(
    VisibilityState visibility_state) const {
  if (visibility_state != AUTO_HIDE || !launcher_widget())
    return AUTO_HIDE_HIDDEN;

  Shell* shell = Shell::GetInstance();
  if (shell->GetAppListTargetVisibility())
    return AUTO_HIDE_SHOWN;

  if (shell->tray() && shell->tray()->should_show_launcher())
    return AUTO_HIDE_SHOWN;

  if (launcher_ && launcher_->IsShowingMenu())
    return AUTO_HIDE_SHOWN;

  if (launcher_widget()->IsActive() || status_->IsActive())
    return AUTO_HIDE_SHOWN;

  // Don't show if the user is dragging the mouse.
  if (event_filter_.get() && event_filter_->in_mouse_drag())
    return AUTO_HIDE_HIDDEN;

  aura::RootWindow* root = launcher_widget()->GetNativeView()->GetRootWindow();
  bool mouse_over_launcher =
      launcher_widget()->GetWindowScreenBounds().Contains(
          root->last_mouse_location());
  return mouse_over_launcher ? AUTO_HIDE_SHOWN : AUTO_HIDE_HIDDEN;
}

void ShelfLayoutManager::UpdateHitTestBounds() {
  gfx::Insets insets;
  // Only modify the hit test when the shelf is visible, so we don't mess with
  // hover hit testing in the auto-hide state.
  if (state_.visibility_state == VISIBLE) {
    // Let clicks at the very top of the launcher through so windows can be
    // resized with the bottom-right corner and bottom edge.
    switch (alignment_) {
      case SHELF_ALIGNMENT_BOTTOM:
        insets.Set(kWorkspaceAreaBottomInset, 0, 0, 0);
        break;
      case SHELF_ALIGNMENT_LEFT:
        insets.Set(0, 0, 0, kWorkspaceAreaBottomInset);
        break;
      case SHELF_ALIGNMENT_RIGHT:
        insets.Set(0, kWorkspaceAreaBottomInset, 0, 0);
        break;
    }
  }
  if (launcher_widget() && launcher_widget()->GetNativeWindow())
    launcher_widget()->GetNativeWindow()->set_hit_test_bounds_override_outer(
        insets);
  status_->GetNativeWindow()->set_hit_test_bounds_override_outer(insets);
}

bool ShelfLayoutManager::IsShelfWindow(aura::Window* window) {
  if (!window)
    return false;
  return (launcher_widget() &&
          launcher_widget()->GetNativeWindow()->Contains(window)) ||
      (status_ && status_->GetNativeWindow()->Contains(window));
}

int ShelfLayoutManager::GetWorkAreaSize(const State& state, int size) const {
  if (state.visibility_state == VISIBLE)
    return size;
  if (state.visibility_state == AUTO_HIDE)
    return kAutoHideSize;
  return 0;
}

}  // namespace internal
}  // namespace ash
