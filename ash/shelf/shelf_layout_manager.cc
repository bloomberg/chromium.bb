// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"

#include <algorithm>
#include <cmath>

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_types.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace/workspace_animations.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_handler.h"
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

// To avoid hiding the shelf when the mouse transitions from a message bubble
// into the shelf, the hit test area is enlarged by this amount of pixels to
// keep the shelf from hiding.
const int kNotificationBubbleGapHeight = 6;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

bool IsDraggingTrayEnabled() {
  static bool dragging_tray_allowed = CommandLine::ForCurrentProcess()->
      HasSwitch(ash::switches::kAshEnableTrayDragging);
  return dragging_tray_allowed;
}

}  // namespace

// static
const int ShelfLayoutManager::kWorkspaceAreaBottomInset = 2;

// static
const int ShelfLayoutManager::kAutoHideSize = 3;

// ShelfLayoutManager::AutoHideEventFilter -------------------------------------

// Notifies ShelfLayoutManager any time the mouse moves.
class ShelfLayoutManager::AutoHideEventFilter : public ui::EventHandler {
 public:
  explicit AutoHideEventFilter(ShelfLayoutManager* shelf);
  virtual ~AutoHideEventFilter();

  // Returns true if the last mouse event was a mouse drag.
  bool in_mouse_drag() const { return in_mouse_drag_; }

  // Overridden from ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;

 private:
  ShelfLayoutManager* shelf_;
  bool in_mouse_drag_;

  DISALLOW_COPY_AND_ASSIGN(AutoHideEventFilter);
};

ShelfLayoutManager::AutoHideEventFilter::AutoHideEventFilter(
    ShelfLayoutManager* shelf)
    : shelf_(shelf),
      in_mouse_drag_(false) {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

ShelfLayoutManager::AutoHideEventFilter::~AutoHideEventFilter() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void ShelfLayoutManager::AutoHideEventFilter::OnMouseEvent(
    ui::MouseEvent* event) {
  // This also checks IsShelfWindow() to make sure we don't attempt to hide the
  // shelf if the mouse down occurs on the shelf.
  in_mouse_drag_ = (event->type() == ui::ET_MOUSE_DRAGGED ||
                    (in_mouse_drag_ && event->type() != ui::ET_MOUSE_RELEASED &&
                     event->type() != ui::ET_MOUSE_CAPTURE_CHANGED)) &&
      !shelf_->IsShelfWindow(static_cast<aura::Window*>(event->target()));
  if (event->type() == ui::ET_MOUSE_MOVED)
    shelf_->UpdateAutoHideState();
  return;
}

// ShelfLayoutManager:UpdateShelfObserver --------------------------------------

// UpdateShelfObserver is used to delay updating the background until the
// animation completes.
class ShelfLayoutManager::UpdateShelfObserver
    : public ui::ImplicitAnimationObserver {
 public:
  explicit UpdateShelfObserver(ShelfLayoutManager* shelf) : shelf_(shelf) {
    shelf_->update_shelf_observer_ = this;
  }

  void Detach() {
    shelf_ = NULL;
  }

  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    if (shelf_) {
      shelf_->UpdateShelfBackground(BackgroundAnimator::CHANGE_ANIMATE);
    }
    delete this;
  }

 private:
  virtual ~UpdateShelfObserver() {
    if (shelf_)
      shelf_->update_shelf_observer_ = NULL;
  }

  // Shelf we're in. NULL if deleted before we're deleted.
  ShelfLayoutManager* shelf_;

  DISALLOW_COPY_AND_ASSIGN(UpdateShelfObserver);
};

// ShelfLayoutManager ----------------------------------------------------------

ShelfLayoutManager::ShelfLayoutManager(ShelfWidget* shelf)
    : root_window_(shelf->GetNativeView()->GetRootWindow()),
      in_layout_(false),
      auto_hide_behavior_(SHELF_AUTO_HIDE_BEHAVIOR_NEVER),
      alignment_(SHELF_ALIGNMENT_BOTTOM),
      shelf_(shelf),
      workspace_controller_(NULL),
      window_overlaps_shelf_(false),
      gesture_drag_status_(GESTURE_DRAG_NONE),
      gesture_drag_amount_(0.f),
      gesture_drag_auto_hide_state_(SHELF_AUTO_HIDE_SHOWN),
      update_shelf_observer_(NULL) {
  Shell::GetInstance()->AddShellObserver(this);
  aura::client::GetActivationClient(root_window_)->AddObserver(this);
}

ShelfLayoutManager::~ShelfLayoutManager() {
  if (update_shelf_observer_)
    update_shelf_observer_->Detach();

  FOR_EACH_OBSERVER(Observer, observers_, WillDeleteShelf());
  Shell::GetInstance()->RemoveShellObserver(this);
  aura::client::GetActivationClient(root_window_)->RemoveObserver(this);
}

void ShelfLayoutManager::SetAutoHideBehavior(ShelfAutoHideBehavior behavior) {
  if (auto_hide_behavior_ == behavior)
    return;
  auto_hide_behavior_ = behavior;
  UpdateVisibilityState();
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnAutoHideStateChanged(state_.auto_hide_state));
}

bool ShelfLayoutManager::IsVisible() const {
  return shelf_->status_area_widget()->IsVisible() &&
      (state_.visibility_state == SHELF_VISIBLE ||
       (state_.visibility_state == SHELF_AUTO_HIDE &&
        state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN));
}

bool ShelfLayoutManager::SetAlignment(ShelfAlignment alignment) {
  if (alignment_ == alignment)
    return false;

  alignment_ = alignment;
  if (shelf_->launcher())
    shelf_->launcher()->SetAlignment(alignment);
  shelf_->status_area_widget()->SetShelfAlignment(alignment);
  LayoutShelf();
  return true;
}

gfx::Rect ShelfLayoutManager::GetIdealBounds() {
  // TODO(oshima): this is wrong. Figure out what display shelf is on
  // and everything should be based on it.
  gfx::Rect bounds(
      ScreenAsh::GetDisplayBoundsInParent(shelf_->GetNativeView()));
  int width = 0, height = 0;
  GetShelfSize(&width, &height);
  return SelectValueForShelfAlignment(
      gfx::Rect(bounds.x(), bounds.bottom() - height, bounds.width(), height),
      gfx::Rect(bounds.x(), bounds.y(), width, bounds.height()),
      gfx::Rect(bounds.right() - width, bounds.y(), width, bounds.height()),
      gfx::Rect(bounds.x(), bounds.y(), bounds.width(), height));
}

void ShelfLayoutManager::LayoutShelf() {
  base::AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
  StopAnimating();
  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  GetLayer(shelf_)->SetOpacity(target_bounds.opacity);
  shelf_->SetBounds(
      ScreenAsh::ConvertRectToScreen(
          shelf_->GetNativeView()->parent(),
          target_bounds.shelf_bounds_in_root));
  if (shelf_->launcher())
    shelf_->launcher()->SetLauncherViewBounds(
        target_bounds.launcher_bounds_in_shelf);
  GetLayer(shelf_->status_area_widget())->SetOpacity(target_bounds.opacity);
  // TODO(harrym): Once status area widget is a child view of shelf
  // this can be simplified.
  gfx::Rect status_bounds = target_bounds.status_bounds_in_shelf;
  status_bounds.set_x(status_bounds.x() +
                      target_bounds.shelf_bounds_in_root.x());
  status_bounds.set_y(status_bounds.y() +
                      target_bounds.shelf_bounds_in_root.y());
  shelf_->status_area_widget()->SetBounds(
      ScreenAsh::ConvertRectToScreen(
          shelf_->status_area_widget()->GetNativeView()->parent(),
          status_bounds));
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      root_window_, target_bounds.work_area_insets);
  UpdateHitTestBounds();
}

ShelfVisibilityState ShelfLayoutManager::CalculateShelfVisibility() {
  switch(auto_hide_behavior_) {
    case SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      return SHELF_AUTO_HIDE;
    case SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      return SHELF_VISIBLE;
    case SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
      return SHELF_HIDDEN;
  }
  return SHELF_VISIBLE;
}

ShelfVisibilityState
ShelfLayoutManager::CalculateShelfVisibilityWhileDragging() {
  switch(auto_hide_behavior_) {
    case SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
    case SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      return SHELF_AUTO_HIDE;
    case SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
      return SHELF_HIDDEN;
  }
  return SHELF_VISIBLE;
}

void ShelfLayoutManager::UpdateVisibilityState() {
  ShellDelegate* delegate = Shell::GetInstance()->delegate();
  if (delegate && delegate->IsScreenLocked()) {
    SetState(SHELF_VISIBLE);
  } else if (gesture_drag_status_ == GESTURE_DRAG_COMPLETE_IN_PROGRESS) {
    // TODO(zelidrag): Verify shelf drag animation still shows on the device
    // when we are in SHELF_AUTO_HIDE_ALWAYS_HIDDEN.
    SetState(CalculateShelfVisibilityWhileDragging());
  } else if (GetRootWindowController(root_window_)->IsImmersiveMode()) {
    // The user choosing immersive mode indicates he or she wants to maximize
    // screen real-estate for content, so always auto-hide the shelf.
    DCHECK_NE(auto_hide_behavior_, SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
    SetState(SHELF_AUTO_HIDE);
  } else {
    WorkspaceWindowState window_state(workspace_controller_->GetWindowState());
    switch (window_state) {
      case WORKSPACE_WINDOW_STATE_FULL_SCREEN:
        SetState(SHELF_HIDDEN);
        break;

      case WORKSPACE_WINDOW_STATE_MAXIMIZED:
        SetState(CalculateShelfVisibility());
        break;

      case WORKSPACE_WINDOW_STATE_WINDOW_OVERLAPS_SHELF:
      case WORKSPACE_WINDOW_STATE_DEFAULT:
        SetState(CalculateShelfVisibility());
        SetWindowOverlapsShelf(window_state ==
                               WORKSPACE_WINDOW_STATE_WINDOW_OVERLAPS_SHELF);
        break;
    }
  }
}

void ShelfLayoutManager::UpdateAutoHideState() {
  ShelfAutoHideState auto_hide_state =
      CalculateAutoHideState(state_.visibility_state);
  if (auto_hide_state != state_.auto_hide_state) {
    if (auto_hide_state == SHELF_AUTO_HIDE_HIDDEN) {
      // Hides happen immediately.
      SetState(state_.visibility_state);
      FOR_EACH_OBSERVER(Observer, observers_,
                        OnAutoHideStateChanged(auto_hide_state));
    } else {
      auto_hide_timer_.Stop();
      auto_hide_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kAutoHideDelayMS),
          this, &ShelfLayoutManager::UpdateAutoHideStateNow);
      FOR_EACH_OBSERVER(Observer, observers_, OnAutoHideStateChanged(
          CalculateAutoHideState(state_.visibility_state)));
    }
  } else {
    auto_hide_timer_.Stop();
  }
}

void ShelfLayoutManager::SetWindowOverlapsShelf(bool value) {
  window_overlaps_shelf_ = value;
  UpdateShelfBackground(BackgroundAnimator::CHANGE_ANIMATE);
}

void ShelfLayoutManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ShelfLayoutManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, Gesture dragging:

void ShelfLayoutManager::StartGestureDrag(const ui::GestureEvent& gesture) {
  gesture_drag_status_ = GESTURE_DRAG_IN_PROGRESS;
  gesture_drag_amount_ = 0.f;
  gesture_drag_auto_hide_state_ = visibility_state() == SHELF_AUTO_HIDE ?
      auto_hide_state() : SHELF_AUTO_HIDE_SHOWN;
  UpdateShelfBackground(BackgroundAnimator::CHANGE_ANIMATE);
}

ShelfLayoutManager::DragState ShelfLayoutManager::UpdateGestureDrag(
    const ui::GestureEvent& gesture) {
  bool horizontal = IsHorizontalAlignment();
  gesture_drag_amount_ += horizontal ? gesture.details().scroll_y() :
                                       gesture.details().scroll_x();
  LayoutShelf();

  // Start reveling the status menu when:
  //   - dragging up on an already visible shelf
  //   - dragging up on a hidden shelf, but it is currently completely visible.
  if (horizontal && gesture.details().scroll_y() < 0) {
    int min_height = 0;
    if (gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_HIDDEN && shelf_)
      min_height = shelf_->GetContentsView()->GetPreferredSize().height();

    if (min_height < shelf_->GetWindowBoundsInScreen().height() &&
        gesture.root_location().x() >=
        shelf_->status_area_widget()->GetWindowBoundsInScreen().x() &&
        IsDraggingTrayEnabled())
      return DRAG_TRAY;
  }

  return DRAG_SHELF;
}

void ShelfLayoutManager::CompleteGestureDrag(const ui::GestureEvent& gesture) {
  bool horizontal = IsHorizontalAlignment();
  bool should_change = false;
  if (gesture.type() == ui::ET_GESTURE_SCROLL_END) {
    // The visibility of the shelf changes only if the shelf was dragged X%
    // along the correct axis. If the shelf was already visible, then the
    // direction of the drag does not matter.
    const float kDragHideThreshold = 0.4f;
    gfx::Rect bounds = GetIdealBounds();
    float drag_ratio = fabs(gesture_drag_amount_) /
                       (horizontal ?  bounds.height() : bounds.width());
    if (gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN) {
      should_change = drag_ratio > kDragHideThreshold;
    } else {
      bool correct_direction = false;
      switch (alignment_) {
        case SHELF_ALIGNMENT_BOTTOM:
        case SHELF_ALIGNMENT_RIGHT:
          correct_direction = gesture_drag_amount_ < 0;
          break;
        case SHELF_ALIGNMENT_LEFT:
        case SHELF_ALIGNMENT_TOP:
          correct_direction = gesture_drag_amount_ > 0;
          break;
      }
      should_change = correct_direction && drag_ratio > kDragHideThreshold;
    }
  } else if (gesture.type() == ui::ET_SCROLL_FLING_START) {
    if (gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN) {
      should_change = horizontal ? fabs(gesture.details().velocity_y()) > 0 :
                                   fabs(gesture.details().velocity_x()) > 0;
    } else {
      should_change = SelectValueForShelfAlignment(
          gesture.details().velocity_y() < 0,
          gesture.details().velocity_x() > 0,
          gesture.details().velocity_x() < 0,
          gesture.details().velocity_y() > 0);
    }
  } else {
    NOTREACHED();
  }

  if (!should_change) {
    CancelGestureDrag();
    return;
  }

  gesture_drag_auto_hide_state_ =
      gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN ?
      SHELF_AUTO_HIDE_HIDDEN : SHELF_AUTO_HIDE_SHOWN;
  if (shelf_)
    shelf_->Deactivate();
  shelf_->status_area_widget()->Deactivate();
  if (gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_HIDDEN &&
      auto_hide_behavior_ != SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS) {
    gesture_drag_status_ = GESTURE_DRAG_NONE;
    SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  } else if (gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN &&
             auto_hide_behavior_ != SHELF_AUTO_HIDE_BEHAVIOR_NEVER) {
    gesture_drag_status_ = GESTURE_DRAG_NONE;
    SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  } else {
    gesture_drag_status_ = GESTURE_DRAG_COMPLETE_IN_PROGRESS;
    UpdateVisibilityState();
    gesture_drag_status_ = GESTURE_DRAG_NONE;
  }
}

void ShelfLayoutManager::CancelGestureDrag() {
  gesture_drag_status_ = GESTURE_DRAG_NONE;
  ui::ScopedLayerAnimationSettings
      launcher_settings(GetLayer(shelf_)->GetAnimator()),
      status_settings(GetLayer(shelf_->status_area_widget())->GetAnimator());
  LayoutShelf();
  UpdateVisibilityState();
  UpdateShelfBackground(BackgroundAnimator::CHANGE_ANIMATE);
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
  // We may contain other widgets (such as frame maximize bubble) but they don't
  // effect the layout in anyway.
  if (!in_layout_ &&
      ((shelf_->GetNativeView() == child) ||
       (shelf_->status_area_widget()->GetNativeView() == child))) {
    LayoutShelf();
  }
}

void ShelfLayoutManager::OnLockStateChanged(bool locked) {
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnWindowActivated(aura::Window* gained_active,
                                           aura::Window* lost_active) {
  UpdateAutoHideStateNow();
}

bool ShelfLayoutManager::IsHorizontalAlignment() const {
  return alignment_ == SHELF_ALIGNMENT_BOTTOM ||
         alignment_ == SHELF_ALIGNMENT_TOP;
}

// static
ShelfLayoutManager* ShelfLayoutManager::ForLauncher(aura::Window* window) {
  ShelfWidget* shelf = RootWindowController::ForLauncher(window)->shelf();
  return shelf ? shelf->shelf_layout_manager() : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, private:

ShelfLayoutManager::TargetBounds::TargetBounds() : opacity(0.0f) {}
ShelfLayoutManager::TargetBounds::~TargetBounds() {}

void ShelfLayoutManager::SetState(ShelfVisibilityState visibility_state) {
  ShellDelegate* delegate = Shell::GetInstance()->delegate();
  State state;
  state.visibility_state = visibility_state;
  state.auto_hide_state = CalculateAutoHideState(visibility_state);
  state.is_screen_locked = delegate && delegate->IsScreenLocked();

  // It's possible for SetState() when a window becomes maximized but the state
  // won't have changed value. Do the dimming check before the early exit.
  if (workspace_controller_) {
    shelf_->SetDimsShelf(
        (state.visibility_state == SHELF_VISIBLE) &&
          workspace_controller_->GetWindowState() ==
              WORKSPACE_WINDOW_STATE_MAXIMIZED);
  }

  if (state_.Equals(state))
    return;  // Nothing changed.

  FOR_EACH_OBSERVER(Observer, observers_,
                    WillChangeVisibilityState(visibility_state));

  if (state.visibility_state == SHELF_AUTO_HIDE) {
    // When state is SHELF_AUTO_HIDE we need to track when the mouse is over the
    // launcher to unhide the shelf. AutoHideEventFilter does that for us.
    if (!event_filter_.get())
      event_filter_.reset(new AutoHideEventFilter(this));
  } else {
    event_filter_.reset(NULL);
  }

  auto_hide_timer_.Stop();

  // Animating the background when transitioning from auto-hide & hidden to
  // visible is janky. Update the background immediately in this case.
  BackgroundAnimator::ChangeType change_type =
      (state_.visibility_state == SHELF_AUTO_HIDE &&
       state_.auto_hide_state == SHELF_AUTO_HIDE_HIDDEN &&
       state.visibility_state == SHELF_VISIBLE) ?
      BackgroundAnimator::CHANGE_IMMEDIATE : BackgroundAnimator::CHANGE_ANIMATE;
  StopAnimating();

  State old_state = state_;
  state_ = state;
  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);

  ui::ScopedLayerAnimationSettings launcher_animation_setter(
      GetLayer(shelf_)->GetAnimator());
  launcher_animation_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kWorkspaceSwitchTimeMS));
  launcher_animation_setter.SetTweenType(ui::Tween::EASE_OUT);
  GetLayer(shelf_)->SetBounds(
      target_bounds.shelf_bounds_in_root);
  GetLayer(shelf_)->SetOpacity(target_bounds.opacity);
  ui::ScopedLayerAnimationSettings status_animation_setter(
      GetLayer(shelf_->status_area_widget())->GetAnimator());
  status_animation_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kWorkspaceSwitchTimeMS));
  status_animation_setter.SetTweenType(ui::Tween::EASE_OUT);

  // Delay updating the background when going from SHELF_AUTO_HIDE_SHOWN to
  // SHELF_AUTO_HIDE_HIDDEN until the shelf animates out. Otherwise during the
  // animation you see the background change.
  // Also delay the animation when the shelf was hidden, and has just been made
  // visible (e.g. using a gesture-drag).
  bool delay_shelf_update =
      state.visibility_state == SHELF_AUTO_HIDE &&
      state.auto_hide_state == SHELF_AUTO_HIDE_HIDDEN &&
      old_state.visibility_state == SHELF_AUTO_HIDE;

  if (state.visibility_state == SHELF_VISIBLE &&
      old_state.visibility_state == SHELF_AUTO_HIDE &&
      old_state.auto_hide_state == SHELF_AUTO_HIDE_HIDDEN)
    delay_shelf_update = true;

  if (delay_shelf_update) {
    if (update_shelf_observer_)
      update_shelf_observer_->Detach();
    // UpdateShelfBackground deletes itself when the animation is done.
    update_shelf_observer_ = new UpdateShelfObserver(this);
    status_animation_setter.AddObserver(update_shelf_observer_);
  }
  ui::Layer* layer = GetLayer(shelf_->status_area_widget());
  // TODO(harrym): Remove when status_area is view (crbug.com/180422).
  gfx::Rect status_bounds = target_bounds.status_bounds_in_shelf;
  status_bounds.set_x(status_bounds.x() +
                      target_bounds.shelf_bounds_in_root.x());
  status_bounds.set_y(status_bounds.y() +
                      target_bounds.shelf_bounds_in_root.y());
  layer->SetBounds(status_bounds);
  layer->SetOpacity(target_bounds.opacity);
  Shell::GetInstance()->SetDisplayWorkAreaInsets(
      root_window_, target_bounds.work_area_insets);
  UpdateHitTestBounds();
  if (!delay_shelf_update)
    UpdateShelfBackground(change_type);
}

void ShelfLayoutManager::StopAnimating() {
  GetLayer(shelf_)->GetAnimator()->StopAnimating();
  GetLayer(shelf_->status_area_widget())->GetAnimator()->StopAnimating();
}

void ShelfLayoutManager::GetShelfSize(int* width, int* height) {
  *width = *height = 0;
  gfx::Size status_size(
      shelf_->status_area_widget()->GetWindowBoundsInScreen().size());
  if (IsHorizontalAlignment())
    *height = std::max(kLauncherPreferredSize, status_size.height());
  else
    *width = std::max(kLauncherPreferredSize, status_size.width());
}

void ShelfLayoutManager::AdjustBoundsBasedOnAlignment(int inset,
                                                      gfx::Rect* bounds) const {
  bounds->Inset(SelectValueForShelfAlignment(
      gfx::Insets(0, 0, inset, 0),
      gfx::Insets(0, inset, 0, 0),
      gfx::Insets(0, 0, 0, inset),
      gfx::Insets(inset, 0, 0, 0)));
}

void ShelfLayoutManager::CalculateTargetBounds(
    const State& state,
    TargetBounds* target_bounds) {
  const gfx::Rect& available_bounds(root_window_->bounds());
  gfx::Rect status_size(
      shelf_->status_area_widget()->GetWindowBoundsInScreen().size());
  int shelf_width = 0, shelf_height = 0;
  GetShelfSize(&shelf_width, &shelf_height);
  if (IsHorizontalAlignment())
    shelf_width = available_bounds.width();
  else
    shelf_height = available_bounds.height();

  if (state.visibility_state == SHELF_AUTO_HIDE &&
      state.auto_hide_state == SHELF_AUTO_HIDE_HIDDEN) {
    // Keep the launcher to its full height when dragging is in progress.
    if (gesture_drag_status_ == GESTURE_DRAG_NONE) {
      if (IsHorizontalAlignment())
        shelf_height = kAutoHideSize;
      else
        shelf_width = kAutoHideSize;
    }
  } else if (state.visibility_state == SHELF_HIDDEN) {
    if (IsHorizontalAlignment())
      shelf_height = 0;
    else
      shelf_width = 0;
  }
  target_bounds->shelf_bounds_in_root = SelectValueForShelfAlignment(
      gfx::Rect(available_bounds.x(), available_bounds.bottom() - shelf_height,
                    available_bounds.width(), shelf_height),
      gfx::Rect(available_bounds.x(), available_bounds.y(),
                    shelf_width, available_bounds.height()),
      gfx::Rect(available_bounds.right() - shelf_width, available_bounds.y(),
                    shelf_width, available_bounds.height()),
      gfx::Rect(available_bounds.x(), available_bounds.y(),
                    available_bounds.width(), shelf_height));

  int status_inset = (kLauncherPreferredSize -
      PrimaryAxisValue(status_size.height(), status_size.width()));

  target_bounds->status_bounds_in_shelf = SelectValueForShelfAlignment(
      gfx::Rect(base::i18n::IsRTL() ? 0 : shelf_width - status_size.width(),
                    status_inset, status_size.width(), status_size.height()),
      gfx::Rect(shelf_width - (status_size.width() + status_inset),
                    shelf_height - status_size.height(), status_size.width(),
                    status_size.height()),
      gfx::Rect(status_inset, shelf_height - status_size.height(),
                    status_size.width(), status_size.height()),
      gfx::Rect(base::i18n::IsRTL() ? 0 : shelf_width - status_size.width(),
                    shelf_height - (status_size.height() + status_inset),
                    status_size.width(), status_size.height()));

  target_bounds->launcher_bounds_in_shelf = SelectValueForShelfAlignment(
      gfx::Rect(base::i18n::IsRTL() ? status_size.width() : 0, 0,
                    shelf_width - status_size.width(), shelf_height),
      gfx::Rect(0, 0, shelf_width, shelf_height - status_size.height()),
      gfx::Rect(0, 0, shelf_width, shelf_height - status_size.height()),
      gfx::Rect(base::i18n::IsRTL() ? status_size.width() : 0, 0,
                    shelf_width - status_size.width(), shelf_height));

  target_bounds->work_area_insets = SelectValueForShelfAlignment(
      gfx::Insets(0, 0, GetWorkAreaSize(state, shelf_height), 0),
      gfx::Insets(0, GetWorkAreaSize(state, shelf_width), 0, 0),
      gfx::Insets(0, 0, 0, GetWorkAreaSize(state, shelf_width)),
      gfx::Insets(GetWorkAreaSize(state, shelf_height), 0, 0, 0));

  target_bounds->opacity =
      (gesture_drag_status_ != GESTURE_DRAG_NONE ||
       state.visibility_state == SHELF_VISIBLE ||
       state.visibility_state == SHELF_AUTO_HIDE) ? 1.0f : 0.0f;
  if (gesture_drag_status_ == GESTURE_DRAG_IN_PROGRESS)
    UpdateTargetBoundsForGesture(target_bounds);
}

void ShelfLayoutManager::UpdateTargetBoundsForGesture(
    TargetBounds* target_bounds) const {
  CHECK_EQ(GESTURE_DRAG_IN_PROGRESS, gesture_drag_status_);
  bool horizontal = IsHorizontalAlignment();
  const gfx::Rect& available_bounds(root_window_->bounds());
  int resistance_free_region = 0;

  if (gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_HIDDEN &&
      visibility_state() == SHELF_AUTO_HIDE &&
      auto_hide_state() != SHELF_AUTO_HIDE_SHOWN) {
    // If the shelf was hidden when the drag started (and the state hasn't
    // changed since then, e.g. because the tray-menu was shown because of the
    // drag), then allow the drag some resistance-free region at first to make
    // sure the shelf sticks with the finger until the shelf is visible.
    resistance_free_region = kLauncherPreferredSize - kAutoHideSize;
  }

  bool resist = SelectValueForShelfAlignment(
      gesture_drag_amount_ < -resistance_free_region,
      gesture_drag_amount_ > resistance_free_region,
      gesture_drag_amount_ < -resistance_free_region,
      gesture_drag_amount_ > resistance_free_region);

  float translate = 0.f;
  if (resist) {
    float diff = fabsf(gesture_drag_amount_) - resistance_free_region;
    diff = std::min(diff, sqrtf(diff));
    if (gesture_drag_amount_ < 0)
      translate = -resistance_free_region - diff;
    else
      translate = resistance_free_region + diff;
  } else {
    translate = gesture_drag_amount_;
  }

  if (horizontal) {
    // Move and size the launcher with the gesture.
    if (alignment_ == SHELF_ALIGNMENT_BOTTOM) {
      target_bounds->shelf_bounds_in_root.Offset(0, translate);
      target_bounds->shelf_bounds_in_root.set_height(
          available_bounds.bottom() - target_bounds->shelf_bounds_in_root.y());
    } else {
      target_bounds->shelf_bounds_in_root.set_height(
          target_bounds->shelf_bounds_in_root.height() + translate);
    }

    // The statusbar should be in the center of the shelf.
    gfx::Rect status_y = target_bounds->shelf_bounds_in_root;
    status_y.set_y(0);
    status_y.ClampToCenteredSize(
        target_bounds->status_bounds_in_shelf.size());
    target_bounds->status_bounds_in_shelf.set_y(status_y.y());
  } else {
    // Move and size the launcher with the gesture.
    if (alignment_ == SHELF_ALIGNMENT_RIGHT) {
      target_bounds->shelf_bounds_in_root.Offset(translate, 0);
      target_bounds->shelf_bounds_in_root.set_width(
          available_bounds.right() - target_bounds->shelf_bounds_in_root.x());
    } else {
      target_bounds->shelf_bounds_in_root.set_width(
          target_bounds->shelf_bounds_in_root.width() + translate);
    }

    // The statusbar should be in the center of the shelf.
    gfx::Rect status_x = target_bounds->shelf_bounds_in_root;
    status_x.set_x(0);
    status_x.ClampToCenteredSize(
        target_bounds->status_bounds_in_shelf.size());
    target_bounds->status_bounds_in_shelf.set_x(status_x.x());
  }
}

void ShelfLayoutManager::UpdateShelfBackground(
    BackgroundAnimator::ChangeType type) {
  bool launcher_paints = GetLauncherPaintsBackground();
  shelf_->SetPaintsBackground(launcher_paints, type);
}

bool ShelfLayoutManager::GetLauncherPaintsBackground() const {
  return gesture_drag_status_ != GESTURE_DRAG_NONE ||
      (!state_.is_screen_locked && window_overlaps_shelf_) ||
      (state_.visibility_state == SHELF_AUTO_HIDE) ;
}

void ShelfLayoutManager::UpdateAutoHideStateNow() {
  SetState(state_.visibility_state);
}

ShelfAutoHideState ShelfLayoutManager::CalculateAutoHideState(
    ShelfVisibilityState visibility_state) const {
  if (visibility_state != SHELF_AUTO_HIDE || !shelf_)
    return SHELF_AUTO_HIDE_HIDDEN;

  if (gesture_drag_status_ == GESTURE_DRAG_COMPLETE_IN_PROGRESS)
    return gesture_drag_auto_hide_state_;

  Shell* shell = Shell::GetInstance();
  if (shell->GetAppListTargetVisibility())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_->status_area_widget() &&
      shelf_->status_area_widget()->ShouldShowLauncher())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_->launcher() && shelf_->launcher()->IsShowingMenu())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_->launcher() && shelf_->launcher()->IsShowingOverflowBubble())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_->IsActive() || shelf_->status_area_widget()->IsActive())
    return SHELF_AUTO_HIDE_SHOWN;

  // Don't show if the user is dragging the mouse.
  if (event_filter_.get() && event_filter_->in_mouse_drag())
    return SHELF_AUTO_HIDE_HIDDEN;

  gfx::Rect shelf_region = shelf_->GetWindowBoundsInScreen();
  if (shelf_->status_area_widget() &&
      shelf_->status_area_widget()->IsMessageBubbleShown() &&
      IsVisible()) {
    // Increase the the hit test area to prevent the shelf from disappearing
    // when the mouse is over the bubble gap.
    shelf_region.Inset(alignment_ == SHELF_ALIGNMENT_RIGHT ?
                           -kNotificationBubbleGapHeight : 0,
                       alignment_ == SHELF_ALIGNMENT_BOTTOM ?
                           -kNotificationBubbleGapHeight : 0,
                       alignment_ == SHELF_ALIGNMENT_LEFT ?
                           -kNotificationBubbleGapHeight : 0,
                       alignment_ == SHELF_ALIGNMENT_TOP ?
                           -kNotificationBubbleGapHeight : 0);
  }

  if (shelf_region.Contains(Shell::GetScreen()->GetCursorScreenPoint()))
    return SHELF_AUTO_HIDE_SHOWN;

  const std::vector<aura::Window*> windows =
      ash::WindowCycleController::BuildWindowList(NULL);

  // Process the window list and check if there are any visible windows.
  for (size_t i = 0; i < windows.size(); ++i) {
    if (windows[i] && windows[i]->IsVisible() &&
        !ash::wm::IsWindowMinimized(windows[i]))
      return SHELF_AUTO_HIDE_HIDDEN;
  }

  // If there are no visible windows do not hide the shelf.
  return SHELF_AUTO_HIDE_SHOWN;
}

void ShelfLayoutManager::UpdateHitTestBounds() {
  gfx::Insets insets;
  // Only modify the hit test when the shelf is visible, so we don't mess with
  // hover hit testing in the auto-hide state.
  if (state_.visibility_state == SHELF_VISIBLE) {
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
      case SHELF_ALIGNMENT_TOP:
        insets.Set(0, 0, kWorkspaceAreaBottomInset, 0);
        break;
    }
  }
  if (shelf_ && shelf_->GetNativeWindow()) {
    shelf_->GetNativeWindow()->SetHitTestBoundsOverrideOuter(
        insets, 1);
  }
  shelf_->status_area_widget()->GetNativeWindow()->
      SetHitTestBoundsOverrideOuter(insets, 1);
}

bool ShelfLayoutManager::IsShelfWindow(aura::Window* window) {
  if (!window)
    return false;
  return (shelf_ &&
          shelf_->GetNativeWindow()->Contains(window)) ||
      (shelf_->status_area_widget()->GetNativeWindow()->Contains(window));
}

int ShelfLayoutManager::GetWorkAreaSize(const State& state, int size) const {
  if (state.visibility_state == SHELF_VISIBLE)
    return size;
  if (state.visibility_state == SHELF_AUTO_HIDE)
    return kAutoHideSize;
  return 0;
}

}  // namespace internal
}  // namespace ash
