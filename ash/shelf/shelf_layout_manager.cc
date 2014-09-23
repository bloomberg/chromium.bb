// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_bezel_event_filter.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/gestures/shelf_gesture_handler.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/screen.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Delay before showing the shelf. This is after the mouse stops moving.
const int kAutoHideDelayMS = 200;

// To avoid hiding the shelf when the mouse transitions from a message bubble
// into the shelf, the hit test area is enlarged by this amount of pixels to
// keep the shelf from hiding.
const int kNotificationBubbleGapHeight = 6;

// The maximum size of the region on the display opposing the shelf managed by
// this ShelfLayoutManager which can trigger showing the shelf.
// For instance:
// - Primary display is left of secondary display.
// - Shelf is left aligned
// - This ShelfLayoutManager manages the shelf for the secondary display.
// |kMaxAutoHideShowShelfRegionSize| refers to the maximum size of the region
// from the right edge of the primary display which can trigger showing the
// auto hidden shelf. The region is used to make it easier to trigger showing
// the auto hidden shelf when the shelf is on the boundary between displays.
const int kMaxAutoHideShowShelfRegionSize = 10;

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
const int ShelfLayoutManager::kWorkspaceAreaVisibleInset = 2;

// static
const int ShelfLayoutManager::kWorkspaceAreaAutoHideInset = 5;

// static
const int ShelfLayoutManager::kAutoHideSize = 3;

// static
const int ShelfLayoutManager::kShelfItemInset = 3;

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
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  ShelfLayoutManager* shelf_;
  bool in_mouse_drag_;
  ShelfGestureHandler gesture_handler_;
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

void ShelfLayoutManager::AutoHideEventFilter::OnGestureEvent(
    ui::GestureEvent* event) {
  if (shelf_->IsShelfWindow(static_cast<aura::Window*>(event->target()))) {
    if (gesture_handler_.ProcessGestureEvent(*event))
      event->StopPropagation();
  }
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
    if (shelf_)
      shelf_->UpdateShelfBackground(BACKGROUND_CHANGE_ANIMATE);
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
    : SnapToPixelLayoutManager(shelf->GetNativeView()->parent()),
      root_window_(shelf->GetNativeView()->GetRootWindow()),
      updating_bounds_(false),
      auto_hide_behavior_(SHELF_AUTO_HIDE_BEHAVIOR_NEVER),
      alignment_(SHELF_ALIGNMENT_BOTTOM),
      shelf_(shelf),
      workspace_controller_(NULL),
      window_overlaps_shelf_(false),
      mouse_over_shelf_when_auto_hide_timer_started_(false),
      bezel_event_filter_(new ShelfBezelEventFilter(this)),
      gesture_drag_status_(GESTURE_DRAG_NONE),
      gesture_drag_amount_(0.f),
      gesture_drag_auto_hide_state_(SHELF_AUTO_HIDE_SHOWN),
      update_shelf_observer_(NULL),
      duration_override_in_ms_(0) {
  Shell::GetInstance()->AddShellObserver(this);
  Shell::GetInstance()->lock_state_controller()->AddObserver(this);
  aura::client::GetActivationClient(root_window_)->AddObserver(this);
  Shell::GetInstance()->session_state_delegate()->AddSessionStateObserver(this);
}

ShelfLayoutManager::~ShelfLayoutManager() {
  if (update_shelf_observer_)
    update_shelf_observer_->Detach();

  FOR_EACH_OBSERVER(ShelfLayoutManagerObserver, observers_, WillDeleteShelf());
  Shell::GetInstance()->RemoveShellObserver(this);
  Shell::GetInstance()->lock_state_controller()->RemoveObserver(this);
  aura::client::GetActivationClient(root_window_)->RemoveObserver(this);
  Shell::GetInstance()->
      session_state_delegate()->RemoveSessionStateObserver(this);
}

void ShelfLayoutManager::SetAutoHideBehavior(ShelfAutoHideBehavior behavior) {
  if (auto_hide_behavior_ == behavior)
    return;
  auto_hide_behavior_ = behavior;
  UpdateVisibilityState();
  FOR_EACH_OBSERVER(ShelfLayoutManagerObserver, observers_,
                    OnAutoHideBehaviorChanged(root_window_,
                                              auto_hide_behavior_));
}

void ShelfLayoutManager::PrepareForShutdown() {
  // Clear all event filters, otherwise sometimes those filters may catch
  // synthesized mouse event and cause crashes during the shutdown.
  set_workspace_controller(NULL);
  auto_hide_event_filter_.reset();
  bezel_event_filter_.reset();
}

bool ShelfLayoutManager::IsVisible() const {
  // status_area_widget() may be NULL during the shutdown.
  return shelf_->status_area_widget() &&
      shelf_->status_area_widget()->IsVisible() &&
      (state_.visibility_state == SHELF_VISIBLE ||
       (state_.visibility_state == SHELF_AUTO_HIDE &&
        state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN));
}

bool ShelfLayoutManager::SetAlignment(ShelfAlignment alignment) {
  if (alignment_ == alignment)
    return false;

  // This should not be called during the lock screen transitions.
  DCHECK(!Shell::GetInstance()->session_state_delegate()->IsScreenLocked());
  alignment_ = alignment;
  shelf_->SetAlignment(alignment);
  LayoutShelf();
  return true;
}

ShelfAlignment ShelfLayoutManager::GetAlignment() const {
  // When the screen is locked, the shelf is forced into bottom alignment.
  if (Shell::GetInstance()->session_state_delegate()->IsScreenLocked())
    return SHELF_ALIGNMENT_BOTTOM;
  return alignment_;
}

gfx::Rect ShelfLayoutManager::GetIdealBounds() {
  gfx::Rect bounds(
      ScreenUtil::GetDisplayBoundsInParent(shelf_->GetNativeView()));
  int width = 0, height = 0;
  GetShelfSize(&width, &height);
  return SelectValueForShelfAlignment(
      gfx::Rect(bounds.x(), bounds.bottom() - height, bounds.width(), height),
      gfx::Rect(bounds.x(), bounds.y(), width, bounds.height()),
      gfx::Rect(bounds.right() - width, bounds.y(), width, bounds.height()),
      gfx::Rect(bounds.x(), bounds.y(), bounds.width(), height));
}

void ShelfLayoutManager::LayoutShelf() {
  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  UpdateBoundsAndOpacity(target_bounds, false, NULL);

  if (shelf_->shelf()) {
    // This is not part of UpdateBoundsAndOpacity() because
    // SetShelfViewBounds() sets the bounds immediately and does not animate.
    // The height of the ShelfView for a horizontal shelf and the width of
    // the ShelfView for a vertical shelf are set when |shelf_|'s bounds
    // are changed via UpdateBoundsAndOpacity(). This sets the origin and the
    // dimension in the other direction.
    shelf_->shelf()->SetShelfViewBounds(
        target_bounds.shelf_bounds_in_shelf);
  }
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

void ShelfLayoutManager::UpdateVisibilityState() {
  // Bail out early when there is no |workspace_controller_|, which happens
  // during shutdown after PrepareForShutdown.
  if (!workspace_controller_)
    return;

  if (Shell::GetInstance()->session_state_delegate()->IsScreenLocked()) {
    SetState(SHELF_VISIBLE);
  } else {
    // TODO(zelidrag): Verify shelf drag animation still shows on the device
    // when we are in SHELF_AUTO_HIDE_ALWAYS_HIDDEN.
    WorkspaceWindowState window_state(workspace_controller_->GetWindowState());
    switch (window_state) {
      case WORKSPACE_WINDOW_STATE_FULL_SCREEN: {
        const aura::Window* fullscreen_window = GetRootWindowController(
            root_window_)->GetWindowForFullscreenMode();
        if (fullscreen_window && wm::GetWindowState(fullscreen_window)->
                hide_shelf_when_fullscreen()) {
          SetState(SHELF_HIDDEN);
        } else {
          // The shelf is sometimes not hidden when in immersive fullscreen.
          // Force the shelf to be auto hidden in this case.
          SetState(SHELF_AUTO_HIDE);
        }
        break;
      }

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
    } else {
      if (!auto_hide_timer_.IsRunning()) {
        mouse_over_shelf_when_auto_hide_timer_started_ =
            shelf_->GetWindowBoundsInScreen().Contains(
                Shell::GetScreen()->GetCursorScreenPoint());
      }
      auto_hide_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kAutoHideDelayMS),
          this, &ShelfLayoutManager::UpdateAutoHideStateNow);
    }
  } else {
    StopAutoHideTimer();
  }
}

void ShelfLayoutManager::SetWindowOverlapsShelf(bool value) {
  window_overlaps_shelf_ = value;
  UpdateShelfBackground(BACKGROUND_CHANGE_ANIMATE);
}

void ShelfLayoutManager::AddObserver(ShelfLayoutManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ShelfLayoutManager::RemoveObserver(ShelfLayoutManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, Gesture functions:

void ShelfLayoutManager::OnGestureEdgeSwipe(const ui::GestureEvent& gesture) {
  if (visibility_state() == SHELF_AUTO_HIDE) {
    gesture_drag_auto_hide_state_ = SHELF_AUTO_HIDE_SHOWN;
    gesture_drag_status_ = GESTURE_DRAG_COMPLETE_IN_PROGRESS;
    UpdateVisibilityState();
    gesture_drag_status_ = GESTURE_DRAG_NONE;
  }
}

void ShelfLayoutManager::StartGestureDrag(const ui::GestureEvent& gesture) {
  gesture_drag_status_ = GESTURE_DRAG_IN_PROGRESS;
  gesture_drag_amount_ = 0.f;
  gesture_drag_auto_hide_state_ = visibility_state() == SHELF_AUTO_HIDE ?
      auto_hide_state() : SHELF_AUTO_HIDE_SHOWN;
  UpdateShelfBackground(BACKGROUND_CHANGE_ANIMATE);
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
      switch (GetAlignment()) {
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
  if (shelf_) {
    shelf_->Deactivate();
    shelf_->status_area_widget()->Deactivate();
  }
  gesture_drag_auto_hide_state_ =
      gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN ?
      SHELF_AUTO_HIDE_HIDDEN : SHELF_AUTO_HIDE_SHOWN;
  ShelfAutoHideBehavior new_auto_hide_behavior =
      gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN ?
      SHELF_AUTO_HIDE_BEHAVIOR_NEVER : SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;

  // When in fullscreen and the shelf is forced to be auto hidden, the auto hide
  // behavior affects neither the visibility state nor the auto hide state. Set
  // |gesture_drag_status_| to GESTURE_DRAG_COMPLETE_IN_PROGRESS to set the auto
  // hide state to |gesture_drag_auto_hide_state_|.
  gesture_drag_status_ = GESTURE_DRAG_COMPLETE_IN_PROGRESS;
  if (auto_hide_behavior_ != new_auto_hide_behavior)
    SetAutoHideBehavior(new_auto_hide_behavior);
  else
    UpdateVisibilityState();
  gesture_drag_status_ = GESTURE_DRAG_NONE;
}

void ShelfLayoutManager::CancelGestureDrag() {
  gesture_drag_status_ = GESTURE_DRAG_CANCEL_IN_PROGRESS;
  UpdateVisibilityState();
  gesture_drag_status_ = GESTURE_DRAG_NONE;
}

void ShelfLayoutManager::SetAnimationDurationOverride(
    int duration_override_in_ms) {
  duration_override_in_ms_ = duration_override_in_ms;
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, aura::LayoutManager implementation:

void ShelfLayoutManager::OnWindowResized() {
  LayoutShelf();
}

void ShelfLayoutManager::SetChildBounds(aura::Window* child,
                                        const gfx::Rect& requested_bounds) {
  SnapToPixelLayoutManager::SetChildBounds(child, requested_bounds);
  // We may contain other widgets (such as frame maximize bubble) but they don't
  // effect the layout in anyway.
  if (!updating_bounds_ &&
      ((shelf_->GetNativeView() == child) ||
       (shelf_->status_area_widget()->GetNativeView() == child))) {
    LayoutShelf();
  }
}

void ShelfLayoutManager::OnLockStateChanged(bool locked) {
  // Force the shelf to layout for alignment (bottom if locked, restore
  // the previous alignment otherwise).
  state_.is_screen_locked = locked;
  shelf_->SetAlignment(locked ? SHELF_ALIGNMENT_BOTTOM : alignment_);
  UpdateVisibilityState();
  LayoutShelf();
}

void ShelfLayoutManager::OnWindowActivated(aura::Window* gained_active,
                                           aura::Window* lost_active) {
  UpdateAutoHideStateNow();
}

bool ShelfLayoutManager::IsHorizontalAlignment() const {
  return GetAlignment() == SHELF_ALIGNMENT_BOTTOM ||
         GetAlignment() == SHELF_ALIGNMENT_TOP;
}

// static
ShelfLayoutManager* ShelfLayoutManager::ForShelf(aura::Window* window) {
  ShelfWidget* shelf = RootWindowController::ForShelf(window)->shelf();
  return shelf ? shelf->shelf_layout_manager() : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, private:

ShelfLayoutManager::TargetBounds::TargetBounds() : opacity(0.0f) {}
ShelfLayoutManager::TargetBounds::~TargetBounds() {}

void ShelfLayoutManager::SetState(ShelfVisibilityState visibility_state) {
  if (!shelf_->GetNativeView())
    return;

  State state;
  state.visibility_state = visibility_state;
  state.auto_hide_state = CalculateAutoHideState(visibility_state);
  state.window_state = workspace_controller_ ?
      workspace_controller_->GetWindowState() : WORKSPACE_WINDOW_STATE_DEFAULT;

  // Force an update because gesture drags affect the shelf bounds and we
  // should animate back to the normal bounds at the end of a gesture.
  bool force_update =
      (gesture_drag_status_ == GESTURE_DRAG_CANCEL_IN_PROGRESS ||
       gesture_drag_status_ == GESTURE_DRAG_COMPLETE_IN_PROGRESS);

  if (!force_update && state_.Equals(state))
    return;  // Nothing changed.

  FOR_EACH_OBSERVER(ShelfLayoutManagerObserver, observers_,
                    WillChangeVisibilityState(visibility_state));

  if (state.visibility_state == SHELF_AUTO_HIDE) {
    // When state is SHELF_AUTO_HIDE we need to track when the mouse is over the
    // shelf to unhide it. AutoHideEventFilter does that for us.
    if (!auto_hide_event_filter_)
      auto_hide_event_filter_.reset(new AutoHideEventFilter(this));
  } else {
    auto_hide_event_filter_.reset(NULL);
  }

  StopAutoHideTimer();

  State old_state = state_;
  state_ = state;

  BackgroundAnimatorChangeType change_type = BACKGROUND_CHANGE_ANIMATE;
  bool delay_background_change = false;

  // Do not animate the background when:
  // - Going from a hidden / auto hidden shelf in fullscreen to a visible shelf
  //   in maximized mode.
  // - Going from an auto hidden shelf in maximized mode to a visible shelf in
  //   maximized mode.
  if (state.visibility_state == SHELF_VISIBLE &&
      state.window_state == WORKSPACE_WINDOW_STATE_MAXIMIZED &&
      old_state.visibility_state != SHELF_VISIBLE) {
    change_type = BACKGROUND_CHANGE_IMMEDIATE;
  } else {
    // Delay the animation when the shelf was hidden, and has just been made
    // visible (e.g. using a gesture-drag).
    if (state.visibility_state == SHELF_VISIBLE &&
        old_state.visibility_state == SHELF_AUTO_HIDE &&
        old_state.auto_hide_state == SHELF_AUTO_HIDE_HIDDEN) {
      delay_background_change = true;
    }
  }

  if (delay_background_change) {
    if (update_shelf_observer_)
      update_shelf_observer_->Detach();
    // UpdateShelfBackground deletes itself when the animation is done.
    update_shelf_observer_ = new UpdateShelfObserver(this);
  } else {
    UpdateShelfBackground(change_type);
  }

  shelf_->SetDimsShelf(
      state.visibility_state == SHELF_VISIBLE &&
      state.window_state == WORKSPACE_WINDOW_STATE_MAXIMIZED);

  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  UpdateBoundsAndOpacity(target_bounds, true,
      delay_background_change ? update_shelf_observer_ : NULL);

  // OnAutoHideStateChanged Should be emitted when:
  //  - firstly state changed to auto-hide from other state
  //  - or, auto_hide_state has changed
  if ((old_state.visibility_state != state_.visibility_state &&
       state_.visibility_state == SHELF_AUTO_HIDE) ||
      old_state.auto_hide_state != state_.auto_hide_state) {
    FOR_EACH_OBSERVER(ShelfLayoutManagerObserver, observers_,
                      OnAutoHideStateChanged(state_.auto_hide_state));
  }
}

void ShelfLayoutManager::UpdateBoundsAndOpacity(
    const TargetBounds& target_bounds,
    bool animate,
    ui::ImplicitAnimationObserver* observer) {
  base::AutoReset<bool> auto_reset_updating_bounds(&updating_bounds_, true);
  {
    ui::ScopedLayerAnimationSettings shelf_animation_setter(
        GetLayer(shelf_)->GetAnimator());
    ui::ScopedLayerAnimationSettings status_animation_setter(
        GetLayer(shelf_->status_area_widget())->GetAnimator());
    if (animate) {
      int duration = duration_override_in_ms_ ? duration_override_in_ms_ :
                                                kCrossFadeDurationMS;
      shelf_animation_setter.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(duration));
      shelf_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
      shelf_animation_setter.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      status_animation_setter.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(duration));
      status_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
      status_animation_setter.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    } else {
      StopAnimating();
      shelf_animation_setter.SetTransitionDuration(base::TimeDelta());
      status_animation_setter.SetTransitionDuration(base::TimeDelta());
    }
    if (observer)
      status_animation_setter.AddObserver(observer);

    GetLayer(shelf_)->SetOpacity(target_bounds.opacity);
    shelf_->SetBounds(ScreenUtil::ConvertRectToScreen(
        shelf_->GetNativeView()->parent(),
        target_bounds.shelf_bounds_in_root));

    GetLayer(shelf_->status_area_widget())->SetOpacity(
        target_bounds.status_opacity);

    // Having a window which is visible but does not have an opacity is an
    // illegal state. We therefore hide the shelf here if required.
    if (!target_bounds.status_opacity)
      shelf_->status_area_widget()->Hide();
    // Setting visibility during an animation causes the visibility property to
    // animate. Override the animation settings to immediately set the
    // visibility property. Opacity will still animate.

    // TODO(harrym): Once status area widget is a child view of shelf
    // this can be simplified.
    gfx::Rect status_bounds = target_bounds.status_bounds_in_shelf;
    status_bounds.set_x(status_bounds.x() +
                        target_bounds.shelf_bounds_in_root.x());
    status_bounds.set_y(status_bounds.y() +
                        target_bounds.shelf_bounds_in_root.y());
    shelf_->status_area_widget()->SetBounds(
        ScreenUtil::ConvertRectToScreen(
            shelf_->status_area_widget()->GetNativeView()->parent(),
            status_bounds));
    SessionStateDelegate* session_state_delegate =
        Shell::GetInstance()->session_state_delegate();
    if (!state_.is_screen_locked) {
      gfx::Insets insets;
      // If user session is blocked (login to new user session or add user to
      // the existing session - multi-profile) then give 100% of work area only
      // if keyboard is not shown.
      if (!session_state_delegate->IsUserSessionBlocked() ||
          !keyboard_bounds_.IsEmpty()) {
        insets = target_bounds.work_area_insets;
      }
      Shell::GetInstance()->SetDisplayWorkAreaInsets(root_window_, insets);
    }
  }

  // Setting visibility during an animation causes the visibility property to
  // animate. Set the visibility property without an animation.
  if (target_bounds.status_opacity)
    shelf_->status_area_widget()->Show();
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
    *height = kShelfSize;
  else
    *width = kShelfSize;
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
  const gfx::Rect available_bounds(root_window_->bounds());
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
    // Auto-hidden shelf always starts with the default size. If a gesture-drag
    // is in progress, then the call to UpdateTargetBoundsForGesture() below
    // takes care of setting the height properly.
    if (IsHorizontalAlignment())
      shelf_height = kAutoHideSize;
    else
      shelf_width = kAutoHideSize;
  } else if (state.visibility_state == SHELF_HIDDEN ||
      (!keyboard_bounds_.IsEmpty() && !keyboard::IsKeyboardOverscrollEnabled()))
  {
    if (IsHorizontalAlignment())
      shelf_height = 0;
    else
      shelf_width = 0;
  }

  int bottom_shelf_vertical_offset = available_bounds.bottom();
  if (keyboard_bounds_.IsEmpty())
    bottom_shelf_vertical_offset -= shelf_height;
  else
    bottom_shelf_vertical_offset -= keyboard_bounds_.height();

  target_bounds->shelf_bounds_in_root = SelectValueForShelfAlignment(
      gfx::Rect(available_bounds.x(), bottom_shelf_vertical_offset,
                    available_bounds.width(), shelf_height),
      gfx::Rect(available_bounds.x(), available_bounds.y(),
                    shelf_width, available_bounds.height()),
      gfx::Rect(available_bounds.right() - shelf_width, available_bounds.y(),
                    shelf_width, available_bounds.height()),
      gfx::Rect(available_bounds.x(), available_bounds.y(),
                    available_bounds.width(), shelf_height));

  if (IsHorizontalAlignment())
    status_size.set_height(kShelfSize);
  else
    status_size.set_width(kShelfSize);

  target_bounds->status_bounds_in_shelf = SelectValueForShelfAlignment(
      gfx::Rect(base::i18n::IsRTL() ? 0 : shelf_width - status_size.width(),
                    0, status_size.width(), status_size.height()),
      gfx::Rect(shelf_width - status_size.width(),
                    shelf_height - status_size.height(), status_size.width(),
                    status_size.height()),
      gfx::Rect(0, shelf_height - status_size.height(),
                    status_size.width(), status_size.height()),
      gfx::Rect(base::i18n::IsRTL() ? 0 : shelf_width - status_size.width(),
                    shelf_height - status_size.height(),
                    status_size.width(), status_size.height()));

  target_bounds->work_area_insets = SelectValueForShelfAlignment(
      gfx::Insets(0, 0, GetWorkAreaSize(state, shelf_height), 0),
      gfx::Insets(0, GetWorkAreaSize(state, shelf_width), 0, 0),
      gfx::Insets(0, 0, 0, GetWorkAreaSize(state, shelf_width)),
      gfx::Insets(GetWorkAreaSize(state, shelf_height), 0, 0, 0));

  // TODO(varkha): The functionality of managing insets for display areas
  // should probably be pushed to a separate component. This would simplify or
  // remove entirely the dependency on keyboard and dock.

  if (!keyboard_bounds_.IsEmpty() && !keyboard::IsKeyboardOverscrollEnabled()) {
    // Also push in the work area inset for the keyboard if it is visible.
    gfx::Insets keyboard_insets(0, 0, keyboard_bounds_.height(), 0);
    target_bounds->work_area_insets += keyboard_insets;
  }

  // Also push in the work area inset for the dock if it is visible.
  if (!dock_bounds_.IsEmpty()) {
    gfx::Insets dock_insets(
        0, (dock_bounds_.x() > 0 ? 0 : dock_bounds_.width()),
        0, (dock_bounds_.x() > 0 ? dock_bounds_.width() : 0));
    target_bounds->work_area_insets += dock_insets;
  }

  target_bounds->opacity =
      (gesture_drag_status_ == GESTURE_DRAG_IN_PROGRESS ||
       state.visibility_state == SHELF_VISIBLE ||
       state.visibility_state == SHELF_AUTO_HIDE) ? 1.0f : 0.0f;
  target_bounds->status_opacity =
      (state.visibility_state == SHELF_AUTO_HIDE &&
       state.auto_hide_state == SHELF_AUTO_HIDE_HIDDEN &&
       gesture_drag_status_ != GESTURE_DRAG_IN_PROGRESS) ?
      0.0f : target_bounds->opacity;

  if (gesture_drag_status_ == GESTURE_DRAG_IN_PROGRESS)
    UpdateTargetBoundsForGesture(target_bounds);

  // This needs to happen after calling UpdateTargetBoundsForGesture(), because
  // that can change the size of the shelf.
  target_bounds->shelf_bounds_in_shelf = SelectValueForShelfAlignment(
      gfx::Rect(0, 0,
                shelf_width - status_size.width(),
                target_bounds->shelf_bounds_in_root.height()),
      gfx::Rect(0, 0, target_bounds->shelf_bounds_in_root.width(),
                shelf_height - status_size.height()),
      gfx::Rect(0, 0, target_bounds->shelf_bounds_in_root.width(),
                shelf_height - status_size.height()),
      gfx::Rect(0, 0,
                shelf_width - status_size.width(),
                target_bounds->shelf_bounds_in_root.height()));
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
    resistance_free_region = kShelfSize - kAutoHideSize;
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
    // Move and size the shelf with the gesture.
    int shelf_height = target_bounds->shelf_bounds_in_root.height() - translate;
    shelf_height = std::max(shelf_height, kAutoHideSize);
    target_bounds->shelf_bounds_in_root.set_height(shelf_height);
    if (GetAlignment() == SHELF_ALIGNMENT_BOTTOM) {
      target_bounds->shelf_bounds_in_root.set_y(
          available_bounds.bottom() - shelf_height);
    }

    target_bounds->status_bounds_in_shelf.set_y(0);
  } else {
    // Move and size the shelf with the gesture.
    int shelf_width = target_bounds->shelf_bounds_in_root.width();
    bool right_aligned = GetAlignment() == SHELF_ALIGNMENT_RIGHT;
    if (right_aligned)
      shelf_width -= translate;
    else
      shelf_width += translate;
    shelf_width = std::max(shelf_width, kAutoHideSize);
    target_bounds->shelf_bounds_in_root.set_width(shelf_width);
    if (right_aligned) {
      target_bounds->shelf_bounds_in_root.set_x(
          available_bounds.right() - shelf_width);
    }

    if (right_aligned)
      target_bounds->status_bounds_in_shelf.set_x(0);
    else
      target_bounds->status_bounds_in_shelf.set_x(
          target_bounds->shelf_bounds_in_root.width() -
          kShelfSize);
  }
}

void ShelfLayoutManager::UpdateShelfBackground(
    BackgroundAnimatorChangeType type) {
  const ShelfBackgroundType background_type(GetShelfBackgroundType());
  shelf_->SetPaintsBackground(background_type, type);
  FOR_EACH_OBSERVER(ShelfLayoutManagerObserver, observers_,
                    OnBackgroundUpdated(background_type, type));
}

ShelfBackgroundType ShelfLayoutManager::GetShelfBackgroundType() const {
  if (state_.visibility_state != SHELF_AUTO_HIDE &&
      state_.window_state == WORKSPACE_WINDOW_STATE_MAXIMIZED) {
    return SHELF_BACKGROUND_MAXIMIZED;
  }

  if (gesture_drag_status_ == GESTURE_DRAG_IN_PROGRESS ||
      (!state_.is_screen_locked && window_overlaps_shelf_) ||
      (state_.visibility_state == SHELF_AUTO_HIDE)) {
    return SHELF_BACKGROUND_OVERLAP;
  }

  return SHELF_BACKGROUND_DEFAULT;
}

void ShelfLayoutManager::UpdateAutoHideStateNow() {
  SetState(state_.visibility_state);

  // If the state did not change, the auto hide timer may still be running.
  StopAutoHideTimer();
}

void ShelfLayoutManager::StopAutoHideTimer() {
  auto_hide_timer_.Stop();
  mouse_over_shelf_when_auto_hide_timer_started_ = false;
}

gfx::Rect ShelfLayoutManager::GetAutoHideShowShelfRegionInScreen() const {
  gfx::Rect shelf_bounds_in_screen = shelf_->GetWindowBoundsInScreen();
  gfx::Vector2d offset = SelectValueForShelfAlignment(
      gfx::Vector2d(0, shelf_bounds_in_screen.height()),
      gfx::Vector2d(-kMaxAutoHideShowShelfRegionSize, 0),
      gfx::Vector2d(shelf_bounds_in_screen.width(), 0),
      gfx::Vector2d(0, -kMaxAutoHideShowShelfRegionSize));

  gfx::Rect show_shelf_region_in_screen = shelf_bounds_in_screen;
  show_shelf_region_in_screen += offset;
  if (IsHorizontalAlignment())
    show_shelf_region_in_screen.set_height(kMaxAutoHideShowShelfRegionSize);
  else
    show_shelf_region_in_screen.set_width(kMaxAutoHideShowShelfRegionSize);

  // TODO: Figure out if we need any special handling when the keyboard is
  // visible.
  return show_shelf_region_in_screen;
}

ShelfAutoHideState ShelfLayoutManager::CalculateAutoHideState(
    ShelfVisibilityState visibility_state) const {
  if (visibility_state != SHELF_AUTO_HIDE || !shelf_)
    return SHELF_AUTO_HIDE_HIDDEN;

  Shell* shell = Shell::GetInstance();
  if (shell->GetAppListTargetVisibility())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_->status_area_widget() &&
      shelf_->status_area_widget()->ShouldShowShelf())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_->shelf() && shelf_->shelf()->IsShowingMenu())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_->shelf() && shelf_->shelf()->IsShowingOverflowBubble())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_->IsActive() ||
      (shelf_->status_area_widget() &&
       shelf_->status_area_widget()->IsActive()))
    return SHELF_AUTO_HIDE_SHOWN;

  const std::vector<aura::Window*> windows =
      ash::MruWindowTracker::BuildWindowList(false);

  // Process the window list and check if there are any visible windows.
  bool visible_window = false;
  for (size_t i = 0; i < windows.size(); ++i) {
    if (windows[i] && windows[i]->IsVisible() &&
        !wm::GetWindowState(windows[i])->IsMinimized() &&
        root_window_ == windows[i]->GetRootWindow()) {
      visible_window = true;
      break;
    }
  }
  // If there are no visible windows do not hide the shelf.
  if (!visible_window)
    return SHELF_AUTO_HIDE_SHOWN;

  if (gesture_drag_status_ == GESTURE_DRAG_COMPLETE_IN_PROGRESS)
    return gesture_drag_auto_hide_state_;

  // Don't show if the user is dragging the mouse.
  if (auto_hide_event_filter_.get() && auto_hide_event_filter_->in_mouse_drag())
    return SHELF_AUTO_HIDE_HIDDEN;

  // Ignore the mouse position if mouse events are disabled.
  aura::client::CursorClient* cursor_client = aura::client::GetCursorClient(
      shelf_->GetNativeWindow()->GetRootWindow());
  if (!cursor_client->IsMouseEventsEnabled())
    return SHELF_AUTO_HIDE_HIDDEN;

  gfx::Rect shelf_region = shelf_->GetWindowBoundsInScreen();
  if (shelf_->status_area_widget() &&
      shelf_->status_area_widget()->IsMessageBubbleShown() &&
      IsVisible()) {
    // Increase the the hit test area to prevent the shelf from disappearing
    // when the mouse is over the bubble gap.
    ShelfAlignment alignment = GetAlignment();
    shelf_region.Inset(alignment == SHELF_ALIGNMENT_RIGHT ?
                           -kNotificationBubbleGapHeight : 0,
                       alignment == SHELF_ALIGNMENT_BOTTOM ?
                           -kNotificationBubbleGapHeight : 0,
                       alignment == SHELF_ALIGNMENT_LEFT ?
                           -kNotificationBubbleGapHeight : 0,
                       alignment == SHELF_ALIGNMENT_TOP ?
                           -kNotificationBubbleGapHeight : 0);
  }

  gfx::Point cursor_position_in_screen =
      Shell::GetScreen()->GetCursorScreenPoint();
  if (shelf_region.Contains(cursor_position_in_screen))
    return SHELF_AUTO_HIDE_SHOWN;

  // When the shelf is auto hidden and the shelf is on the boundary between two
  // displays, it is hard to trigger showing the shelf. For instance, if a
  // user's primary display is left of their secondary display, it is hard to
  // unautohide a left aligned shelf on the secondary display.
  // It is hard because:
  // - It is hard to stop the cursor in the shelf "light bar" and not overshoot.
  // - The cursor is warped to the other display if the cursor gets to the edge
  //   of the display.
  // Show the shelf if the cursor started on the shelf and the user overshot the
  // shelf slightly to make it easier to show the shelf in this situation. We
  // do not check |auto_hide_timer_|.IsRunning() because it returns false when
  // the timer's task is running.
  if ((state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN ||
       mouse_over_shelf_when_auto_hide_timer_started_) &&
      GetAutoHideShowShelfRegionInScreen().Contains(
          cursor_position_in_screen)) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  return SHELF_AUTO_HIDE_HIDDEN;
}

bool ShelfLayoutManager::IsShelfWindow(aura::Window* window) {
  if (!window)
    return false;
  return (shelf_ && shelf_->GetNativeWindow()->Contains(window)) ||
      (shelf_->status_area_widget() &&
       shelf_->status_area_widget()->GetNativeWindow()->Contains(window));
}

int ShelfLayoutManager::GetWorkAreaSize(const State& state, int size) const {
  if (state.visibility_state == SHELF_VISIBLE)
    return size;
  if (state.visibility_state == SHELF_AUTO_HIDE)
    return kAutoHideSize;
  return 0;
}

void ShelfLayoutManager::OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) {
  bool keyboard_is_about_to_hide = false;
  if (new_bounds.IsEmpty() && !keyboard_bounds_.IsEmpty())
    keyboard_is_about_to_hide = true;

  keyboard_bounds_ = new_bounds;
  OnWindowResized();

  SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();

  // On login screen if keyboard has been just hidden, update bounds just once
  // but ignore target_bounds.work_area_insets since shelf overlaps with login
  // window.
  if (session_state_delegate->IsUserSessionBlocked() &&
      keyboard_is_about_to_hide) {
    Shell::GetInstance()->SetDisplayWorkAreaInsets(root_window_, gfx::Insets());
  }
}

void ShelfLayoutManager::OnDockBoundsChanging(
    const gfx::Rect& dock_bounds,
    DockedWindowLayoutManagerObserver::Reason reason) {
  // Skip shelf layout in case docked notification originates from this class.
  if (reason == DISPLAY_INSETS_CHANGED)
    return;
  if (dock_bounds_ != dock_bounds) {
    dock_bounds_ = dock_bounds;
    OnWindowResized();
    UpdateVisibilityState();
    UpdateShelfBackground(BACKGROUND_CHANGE_ANIMATE);
  }
}

void ShelfLayoutManager::OnLockStateEvent(LockStateObserver::EventType event) {
  if (event == EVENT_LOCK_ANIMATION_STARTED) {
    // Enter the screen locked state.
    state_.is_screen_locked = true;
  }
}

void ShelfLayoutManager::SessionStateChanged(
    SessionStateDelegate::SessionState state) {
  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  UpdateBoundsAndOpacity(target_bounds, true, NULL);
  UpdateVisibilityState();
}

}  // namespace ash
