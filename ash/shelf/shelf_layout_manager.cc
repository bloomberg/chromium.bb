// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "ash/animation/animation_change_type.h"
#include "ash/keyboard/keyboard_observer_register.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/sidebar/sidebar.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Delay before showing the shelf. This is after the mouse stops moving.
constexpr int kAutoHideDelayMS = 200;

// Duration of the animation to show or hide the shelf.
constexpr int kAnimationDurationMS = 200;

// To avoid hiding the shelf when the mouse transitions from a message bubble
// into the shelf, the hit test area is enlarged by this amount of pixels to
// keep the shelf from hiding.
constexpr int kNotificationBubbleGapHeight = 6;

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
constexpr int kMaxAutoHideShowShelfRegionSize = 10;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

// Returns true if the window is in the app list window container.
bool IsAppListWindow(const aura::Window* window) {
  const aura::Window* parent = window->parent();
  return parent && parent->id() == kShellWindowId_AppListContainer;
}

}  // namespace

// ShelfLayoutManager::UpdateShelfObserver -------------------------------------

// UpdateShelfObserver is used to delay updating the background until the
// animation completes.
class ShelfLayoutManager::UpdateShelfObserver
    : public ui::ImplicitAnimationObserver {
 public:
  explicit UpdateShelfObserver(ShelfLayoutManager* shelf) : shelf_(shelf) {
    shelf_->update_shelf_observer_ = this;
  }

  void Detach() { shelf_ = nullptr; }

  void OnImplicitAnimationsCompleted() override {
    if (shelf_)
      shelf_->MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
    delete this;
  }

 private:
  ~UpdateShelfObserver() override {
    if (shelf_)
      shelf_->update_shelf_observer_ = nullptr;
  }

  // Shelf we're in. nullptr if deleted before we're deleted.
  ShelfLayoutManager* shelf_;

  DISALLOW_COPY_AND_ASSIGN(UpdateShelfObserver);
};

ShelfLayoutManager::State::State()
    : visibility_state(SHELF_VISIBLE),
      auto_hide_state(SHELF_AUTO_HIDE_HIDDEN),
      window_state(wm::WORKSPACE_WINDOW_STATE_DEFAULT),
      pre_lock_screen_animation_active(false),
      session_state(session_manager::SessionState::UNKNOWN) {}

bool ShelfLayoutManager::State::IsAddingSecondaryUser() const {
  return session_state == session_manager::SessionState::LOGIN_SECONDARY;
}

bool ShelfLayoutManager::State::IsScreenLocked() const {
  return session_state == session_manager::SessionState::LOCKED;
}

bool ShelfLayoutManager::State::Equals(const State& other) const {
  return other.visibility_state == visibility_state &&
         (visibility_state != SHELF_AUTO_HIDE ||
          other.auto_hide_state == auto_hide_state) &&
         other.window_state == window_state &&
         other.pre_lock_screen_animation_active ==
             pre_lock_screen_animation_active &&
         other.session_state == session_state;
}

// ShelfLayoutManager ----------------------------------------------------------

ShelfLayoutManager::ShelfLayoutManager(ShelfWidget* shelf_widget, Shelf* shelf)
    : updating_bounds_(false),
      shelf_widget_(shelf_widget),
      shelf_(shelf),
      is_background_blur_enabled_(
          app_list::features::IsBackgroundBlurEnabled()),
      keyboard_observer_(this),
      scoped_session_observer_(this) {
  DCHECK(shelf_widget_);
  DCHECK(shelf_);
  Shell::Get()->AddShellObserver(this);
  ShellPort::Get()->AddLockStateObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
  state_.session_state = Shell::Get()->session_controller()->GetSessionState();
}

ShelfLayoutManager::~ShelfLayoutManager() {
  if (update_shelf_observer_)
    update_shelf_observer_->Detach();

  for (auto& observer : observers_)
    observer.WillDeleteShelfLayoutManager();
  Shell::Get()->RemoveShellObserver(this);
  ShellPort::Get()->RemoveLockStateObserver(this);
}

void ShelfLayoutManager::PrepareForShutdown() {
  in_shutdown_ = true;
  // Stop observing changes to avoid updating a partially destructed shelf.
  Shell::Get()->activation_client()->RemoveObserver(this);
}

bool ShelfLayoutManager::IsVisible() const {
  // status_area_widget() may be nullptr during the shutdown.
  return shelf_widget_->status_area_widget() &&
         shelf_widget_->status_area_widget()->IsVisible() &&
         (state_.visibility_state == SHELF_VISIBLE ||
          (state_.visibility_state == SHELF_AUTO_HIDE &&
           state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN));
}

gfx::Rect ShelfLayoutManager::GetIdealBounds() {
  aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  gfx::Rect rect(screen_util::GetDisplayBoundsInParent(shelf_window));
  return SelectValueForShelfAlignment(
      gfx::Rect(rect.x(), rect.bottom() - kShelfSize, rect.width(), kShelfSize),
      gfx::Rect(rect.x(), rect.y(), kShelfSize, rect.height()),
      gfx::Rect(rect.right() - kShelfSize, rect.y(), kShelfSize,
                rect.height()));
}

gfx::Size ShelfLayoutManager::GetPreferredSize() {
  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  return target_bounds.shelf_bounds_in_root.size();
}

void ShelfLayoutManager::LayoutShelfAndUpdateBounds(bool change_work_area) {
  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  UpdateBoundsAndOpacity(target_bounds, false, change_work_area, nullptr);

  // Update insets in ShelfWindowTargeter when shelf bounds change.
  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(visibility_state());
}

void ShelfLayoutManager::LayoutShelf() {
  LayoutShelfAndUpdateBounds(true);
}

ShelfVisibilityState ShelfLayoutManager::CalculateShelfVisibility() {
  switch (shelf_->auto_hide_behavior()) {
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
  // Shelf is not available before login.
  // TODO(crbug.com/701157): Remove this when the login webui fake-shelf is
  // replaced with views.
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return;

  // Bail out early after shelf is destroyed.
  aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  if (in_shutdown_ || !shelf_window)
    return;

  if (state_.IsScreenLocked() || state_.IsAddingSecondaryUser()) {
    SetState(SHELF_VISIBLE);
  } else if (Shell::Get()->screen_pinning_controller()->IsPinned()) {
    SetState(SHELF_HIDDEN);
  } else {
    // TODO(zelidrag): Verify shelf drag animation still shows on the device
    // when we are in SHELF_AUTO_HIDE_ALWAYS_HIDDEN.
    wm::WorkspaceWindowState window_state(
        RootWindowController::ForWindow(shelf_window)
            ->GetWorkspaceWindowState());

    switch (window_state) {
      case wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN: {
        if (IsShelfAutoHideForFullscreenMaximized()) {
          SetState(SHELF_AUTO_HIDE);
        } else if (IsShelfHiddenForFullscreen()) {
          SetState(SHELF_HIDDEN);
        } else {
          // The shelf is sometimes not hidden when in immersive fullscreen.
          // Force the shelf to be auto hidden in this case.
          SetState(SHELF_AUTO_HIDE);
        }
        break;
      }
      case wm::WORKSPACE_WINDOW_STATE_MAXIMIZED:
        if (IsShelfAutoHideForFullscreenMaximized()) {
          SetState(SHELF_AUTO_HIDE);
        } else {
          SetState(CalculateShelfVisibility());
        }
        break;

      case wm::WORKSPACE_WINDOW_STATE_WINDOW_OVERLAPS_SHELF:
      case wm::WORKSPACE_WINDOW_STATE_DEFAULT:
        SetState(CalculateShelfVisibility());
        SetWindowOverlapsShelf(
            window_state == wm::WORKSPACE_WINDOW_STATE_WINDOW_OVERLAPS_SHELF);
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
            shelf_widget_->GetWindowBoundsInScreen().Contains(
                display::Screen::GetScreen()->GetCursorScreenPoint());
      }
      auto_hide_timer_.Start(
          FROM_HERE, base::TimeDelta::FromMilliseconds(kAutoHideDelayMS), this,
          &ShelfLayoutManager::UpdateAutoHideStateNow);
    }
  } else {
    StopAutoHideTimer();
  }
}

void ShelfLayoutManager::UpdateAutoHideForMouseEvent(ui::MouseEvent* event,
                                                     aura::Window* target) {
  // This also checks IsShelfWindow() and IsStatusAreaWindow() to make sure we
  // don't attempt to hide the shelf if the mouse down occurs on the shelf.
  in_mouse_drag_ = (event->type() == ui::ET_MOUSE_DRAGGED ||
                    (in_mouse_drag_ && event->type() != ui::ET_MOUSE_RELEASED &&
                     event->type() != ui::ET_MOUSE_CAPTURE_CHANGED)) &&
                   !IsShelfWindow(target) && !IsStatusAreaWindow(target);

  // Don't update during shutdown because synthetic mouse events (e.g. mouse
  // exit) may be generated during status area widget teardown.
  if (visibility_state() != SHELF_AUTO_HIDE || in_shutdown_)
    return;

  if (event->type() == ui::ET_MOUSE_MOVED ||
      event->type() == ui::ET_MOUSE_ENTERED ||
      event->type() == ui::ET_MOUSE_EXITED) {
    UpdateAutoHideState();
  }
}

void ShelfLayoutManager::ProcessGestureEventOnWindow(ui::GestureEvent* event,
                                                     aura::Window* target) {
  // Skip event processing if shelf widget is fully visible to let the default
  // event dispatching do its work.
  if (IsVisible() || in_shutdown_)
    return;

  if (IsShelfWindow(target)) {
    ui::GestureEvent event_in_screen(*event);
    gfx::Point location_in_screen(event->location());
    ::wm::ConvertPointToScreen(target, &location_in_screen);
    event_in_screen.set_location(location_in_screen);
    if (ProcessGestureEvent(event_in_screen))
      event->StopPropagation();
  }
}

void ShelfLayoutManager::SetWindowOverlapsShelf(bool value) {
  window_overlaps_shelf_ = value;
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::AddObserver(ShelfLayoutManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ShelfLayoutManager::RemoveObserver(ShelfLayoutManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool ShelfLayoutManager::ProcessGestureEvent(
    const ui::GestureEvent& event_in_screen) {
  // The gestures are disabled in the lock/login screen.
  SessionController* controller = Shell::Get()->session_controller();
  if (!controller->NumberOfLoggedInUsers() || controller->IsScreenLocked())
    return false;

  if (IsShelfHiddenForFullscreen())
    return false;

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    StartGestureDrag(event_in_screen);
    return true;
  }

  if (gesture_drag_status_ != GESTURE_DRAG_IN_PROGRESS &&
      gesture_drag_status_ != GESTURE_DRAG_APPLIST_IN_PROGRESS) {
    return false;
  }

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    UpdateGestureDrag(event_in_screen);
    return true;
  }

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_END ||
      event_in_screen.type() == ui::ET_SCROLL_FLING_START) {
    if (gesture_drag_status_ == GESTURE_DRAG_APPLIST_IN_PROGRESS)
      CompleteAppListDrag(event_in_screen);
    else
      CompleteGestureDrag(event_in_screen);
    return true;
  }

  // Unexpected event. Reset the state and let the event fall through.
  CancelGestureDrag();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, wm::WmSnapToPixelLayoutManager implementation:

void ShelfLayoutManager::OnWindowResized() {
  LayoutShelf();
}

void ShelfLayoutManager::SetChildBounds(aura::Window* child,
                                        const gfx::Rect& requested_bounds) {
  wm::WmSnapToPixelLayoutManager::SetChildBounds(child, requested_bounds);
  // We may contain other widgets (such as frame maximize bubble) but they don't
  // effect the layout in anyway.
  if (!updating_bounds_ &&
      ((shelf_widget_->GetNativeWindow() == child) ||
       (shelf_widget_->status_area_widget()->GetNativeWindow() == child))) {
    LayoutShelf();
  }
}

void ShelfLayoutManager::OnShelfAutoHideBehaviorChanged(
    aura::Window* root_window) {
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnPinnedStateChanged(aura::Window* pinned_window) {
  // Shelf needs to be hidden on entering to pinned mode, or restored
  // on exiting from pinned mode.
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnVirtualKeyboardStateChanged(
    bool activated,
    aura::Window* root_window) {
  UpdateKeyboardObserverFromStateChanged(
      activated, root_window, shelf_widget_->GetNativeWindow()->GetRootWindow(),
      &keyboard_observer_);
}

void ShelfLayoutManager::OnAppListVisibilityChanged(bool shown,
                                                    aura::Window* root_window) {
  if (shelf_widget_->GetNativeWindow()->GetRootWindow() != root_window)
    return;

  is_app_list_visible_ = shown;
  MaybeUpdateShelfBackground(AnimationChangeType::IMMEDIATE);
}

void ShelfLayoutManager::OnWindowActivated(ActivationReason reason,
                                           aura::Window* gained_active,
                                           aura::Window* lost_active) {
  UpdateAutoHideStateNow();
}

void ShelfLayoutManager::OnKeyboardAppearanceChanging(
    const keyboard::KeyboardStateDescriptor& state) {
  // If in locked mode, change the work area.
  bool change_work_area = state.is_locked;
  keyboard_bounds_ = state.occluded_bounds;
  LayoutShelfAndUpdateBounds(change_work_area);
}

void ShelfLayoutManager::OnKeyboardAvailabilityChanging(
    const bool is_available) {
  // On login screen if keyboard has been just hidden, update bounds just once
  // but ignore target_bounds.work_area_insets since shelf overlaps with login
  // window.
  if (Shell::Get()->session_controller()->IsUserSessionBlocked() &&
      !is_available) {
    Shell::Get()->SetDisplayWorkAreaInsets(shelf_widget_->GetNativeWindow(),
                                           gfx::Insets());
  }
}

void ShelfLayoutManager::OnKeyboardClosed() {
  keyboard_observer_.RemoveAll();
}

ShelfBackgroundType ShelfLayoutManager::GetShelfBackgroundType() const {
  if (state_.pre_lock_screen_animation_active)
    return SHELF_BACKGROUND_DEFAULT;

  // Handle all non active screen states, including OOBE and pre-login.
  if (state_.session_state != session_manager::SessionState::ACTIVE)
    return SHELF_BACKGROUND_OVERLAP;

  // If the app list is active, hide the shelf background to prevent overlap.
  if (is_app_list_visible_)
    return SHELF_BACKGROUND_APP_LIST;

  if (state_.visibility_state != SHELF_AUTO_HIDE &&
      state_.window_state == wm::WORKSPACE_WINDOW_STATE_MAXIMIZED) {
    return SHELF_BACKGROUND_MAXIMIZED;
  }

  if (gesture_drag_status_ == GESTURE_DRAG_IN_PROGRESS ||
      window_overlaps_shelf_ || state_.visibility_state == SHELF_AUTO_HIDE) {
    return SHELF_BACKGROUND_OVERLAP;
  }

  return SHELF_BACKGROUND_DEFAULT;
}

void ShelfLayoutManager::SetChromeVoxPanelHeight(int height) {
  chromevox_panel_height_ = height;
  LayoutShelf();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, private:

ShelfLayoutManager::TargetBounds::TargetBounds()
    : opacity(0.0f), status_opacity(0.0f) {}

ShelfLayoutManager::TargetBounds::~TargetBounds() = default;

void ShelfLayoutManager::SetState(ShelfVisibilityState visibility_state) {
  State state;
  state.visibility_state = visibility_state;
  state.auto_hide_state = CalculateAutoHideState(visibility_state);

  RootWindowController* controller =
      RootWindowController::ForWindow(shelf_widget_->GetNativeWindow());
  state.window_state = controller ? controller->GetWorkspaceWindowState()
                                  : wm::WORKSPACE_WINDOW_STATE_DEFAULT;
  // Preserve the log in screen states.
  state.session_state = state_.session_state;
  state.pre_lock_screen_animation_active =
      state_.pre_lock_screen_animation_active;

  // Force an update because gesture drags affect the shelf bounds and we
  // should animate back to the normal bounds at the end of a gesture.
  bool force_update =
      (gesture_drag_status_ == GESTURE_DRAG_CANCEL_IN_PROGRESS ||
       gesture_drag_status_ == GESTURE_DRAG_COMPLETE_IN_PROGRESS);

  if (!force_update && state_.Equals(state))
    return;  // Nothing changed.

  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(visibility_state);

  StopAutoHideTimer();

  State old_state = state_;
  state_ = state;

  AnimationChangeType change_type = AnimationChangeType::ANIMATE;
  bool delay_background_change = false;

  // Do not animate the background when:
  // - Going from a hidden / auto hidden shelf in fullscreen to a visible shelf
  //   in tablet mode.
  // - Going from an auto hidden shelf in tablet mode to a visible shelf in
  //   tablet mode.
  if (state.visibility_state == SHELF_VISIBLE &&
      state.window_state == wm::WORKSPACE_WINDOW_STATE_MAXIMIZED &&
      old_state.visibility_state != SHELF_VISIBLE) {
    change_type = AnimationChangeType::IMMEDIATE;
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
    // |update_shelf_observer_| deletes itself when the animation is done.
    update_shelf_observer_ = new UpdateShelfObserver(this);
  } else {
    MaybeUpdateShelfBackground(change_type);
  }

  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  UpdateBoundsAndOpacity(
      target_bounds, true /* animate */, true /* change_work_area */,
      delay_background_change ? update_shelf_observer_ : nullptr);

  // OnAutoHideStateChanged Should be emitted when:
  //  - firstly state changed to auto-hide from other state
  //  - or, auto_hide_state has changed
  if ((old_state.visibility_state != state_.visibility_state &&
       state_.visibility_state == SHELF_AUTO_HIDE) ||
      old_state.auto_hide_state != state_.auto_hide_state) {
    for (auto& observer : observers_)
      observer.OnAutoHideStateChanged(state_.auto_hide_state);
  }
}

void ShelfLayoutManager::UpdateBoundsAndOpacity(
    const TargetBounds& target_bounds,
    bool animate,
    bool change_work_area,
    ui::ImplicitAnimationObserver* observer) {
  StatusAreaWidget* status_widget = shelf_widget_->status_area_widget();
  base::AutoReset<bool> auto_reset_updating_bounds(&updating_bounds_, true);
  {
    ui::ScopedLayerAnimationSettings shelf_animation_setter(
        GetLayer(shelf_widget_)->GetAnimator());
    ui::ScopedLayerAnimationSettings status_animation_setter(
        GetLayer(status_widget)->GetAnimator());
    if (animate) {
      auto duration = base::TimeDelta::FromMilliseconds(kAnimationDurationMS);
      shelf_animation_setter.SetTransitionDuration(duration);
      shelf_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
      shelf_animation_setter.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      status_animation_setter.SetTransitionDuration(duration);
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

    GetLayer(shelf_widget_)->SetOpacity(target_bounds.opacity);
    gfx::Rect shelf_bounds = target_bounds.shelf_bounds_in_root;
    ::wm::ConvertRectToScreen(shelf_widget_->GetNativeWindow()->parent(),
                              &shelf_bounds);
    shelf_widget_->SetBounds(shelf_bounds);

    GetLayer(status_widget)->SetOpacity(target_bounds.status_opacity);

    // Having a window which is visible but does not have an opacity is an
    // illegal state. We therefore hide the shelf here if required.
    if (!target_bounds.status_opacity)
      status_widget->Hide();
    // Setting visibility during an animation causes the visibility property to
    // animate. Override the animation settings to immediately set the
    // visibility property. Opacity will still animate.

    // TODO(harrym): Once status area widget is a child view of shelf
    // this can be simplified.
    gfx::Rect status_bounds = target_bounds.status_bounds_in_shelf;
    status_bounds.Offset(target_bounds.shelf_bounds_in_root.OffsetFromOrigin());
    ::wm::ConvertRectToScreen(status_widget->GetNativeWindow()->parent(),
                              &status_bounds);
    status_widget->SetBounds(status_bounds);

    // For crbug.com/622431, when the shelf alignment is BOTTOM_LOCKED, we
    // don't set display work area, as it is not real user-set alignment.
    if (!state_.IsScreenLocked() && change_work_area &&
        shelf_->alignment() != SHELF_ALIGNMENT_BOTTOM_LOCKED) {
      gfx::Insets insets;
      // If user session is blocked (login to new user session or add user to
      // the existing session - multi-profile) then give 100% of work area only
      // if keyboard is not shown.
      if (!state_.IsAddingSecondaryUser() || !keyboard_bounds_.IsEmpty())
        insets = target_bounds.work_area_insets;
      Shell::Get()->SetDisplayWorkAreaInsets(shelf_widget_->GetNativeWindow(),
                                             insets);
    }
  }

  // Set an empty border to avoid the shelf view and status area overlapping.
  // TODO(msw): Avoid setting bounds of views within the shelf widget here.
  gfx::Rect shelf_bounds = gfx::Rect(target_bounds.shelf_bounds_in_root.size());
  shelf_widget_->GetContentsView()->SetBorder(views::CreateEmptyBorder(
      shelf_bounds.InsetsFrom(target_bounds.shelf_bounds_in_shelf)));
  shelf_widget_->GetContentsView()->Layout();

  // Setting visibility during an animation causes the visibility property to
  // animate. Set the visibility property without an animation.
  if (target_bounds.status_opacity)
    status_widget->Show();
}

void ShelfLayoutManager::StopAnimating() {
  GetLayer(shelf_widget_)->GetAnimator()->StopAnimating();
  GetLayer(shelf_widget_->status_area_widget())->GetAnimator()->StopAnimating();
}

void ShelfLayoutManager::CalculateTargetBounds(const State& state,
                                               TargetBounds* target_bounds) {
  int shelf_size = kShelfSize;
  if (state.visibility_state == SHELF_AUTO_HIDE &&
      state.auto_hide_state == SHELF_AUTO_HIDE_HIDDEN) {
    // Auto-hidden shelf always starts with the default size. If a gesture-drag
    // is in progress, then the call to UpdateTargetBoundsForGesture() below
    // takes care of setting the height properly.
    shelf_size = kShelfAutoHideSize;
  } else if (state.visibility_state == SHELF_HIDDEN ||
             (!keyboard_bounds_.IsEmpty() &&
              !keyboard::IsKeyboardOverscrollEnabled())) {
    shelf_size = 0;
  }

  aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  gfx::Rect available_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf_window);
  available_bounds.Inset(0, chromevox_panel_height_, 0, 0);
  int shelf_width = PrimaryAxisValue(available_bounds.width(), shelf_size);
  int shelf_height = PrimaryAxisValue(shelf_size, available_bounds.height());
  int bottom_shelf_vertical_offset = available_bounds.bottom() - shelf_height;

  gfx::Point shelf_origin = SelectValueForShelfAlignment(
      gfx::Point(available_bounds.x(), bottom_shelf_vertical_offset),
      gfx::Point(available_bounds.x(), available_bounds.y()),
      gfx::Point(available_bounds.right() - shelf_width, available_bounds.y()));
  target_bounds->shelf_bounds_in_root =
      gfx::Rect(shelf_origin.x(), shelf_origin.y(), shelf_width, shelf_height);

  gfx::Size status_size(
      shelf_widget_->status_area_widget()->GetWindowBoundsInScreen().size());
  if (shelf_->IsHorizontalAlignment())
    status_size.set_height(kShelfSize);
  else
    status_size.set_width(kShelfSize);

  gfx::Point status_origin = SelectValueForShelfAlignment(
      gfx::Point(0, 0),
      gfx::Point(shelf_width - status_size.width(),
                 shelf_height - status_size.height()),
      gfx::Point(0, shelf_height - status_size.height()));
  if (shelf_->IsHorizontalAlignment() && !base::i18n::IsRTL())
    status_origin.set_x(shelf_width - status_size.width());
  target_bounds->status_bounds_in_shelf = gfx::Rect(status_origin, status_size);

  target_bounds->work_area_insets = SelectValueForShelfAlignment(
      gfx::Insets(0, 0, GetWorkAreaInsets(state, shelf_height), 0),
      gfx::Insets(0, GetWorkAreaInsets(state, shelf_width), 0, 0),
      gfx::Insets(0, 0, 0, GetWorkAreaInsets(state, shelf_width)));

  // TODO(varkha): The functionality of managing insets for display areas
  // should probably be pushed to a separate component. This would simplify or
  // remove entirely the dependency on keyboard and dock.

  if (!keyboard_bounds_.IsEmpty() && !keyboard::IsKeyboardOverscrollEnabled()) {
    // Also push in the work area inset for the keyboard if it is visible.
    gfx::Insets keyboard_insets(0, 0, keyboard_bounds_.height(), 0);
    target_bounds->work_area_insets += keyboard_insets;
  }

  // Also push in the work area insets for the ChromeVox panel if it's visible.
  if (chromevox_panel_height_) {
    gfx::Insets chromevox_insets(chromevox_panel_height_, 0, 0, 0);
    target_bounds->work_area_insets += chromevox_insets;
  }

  target_bounds->opacity = ComputeTargetOpacity(state);
  target_bounds->status_opacity =
      (state.visibility_state == SHELF_AUTO_HIDE &&
       state.auto_hide_state == SHELF_AUTO_HIDE_HIDDEN &&
       gesture_drag_status_ != GESTURE_DRAG_IN_PROGRESS)
          ? 0.0f
          : target_bounds->opacity;

  if (gesture_drag_status_ == GESTURE_DRAG_IN_PROGRESS)
    UpdateTargetBoundsForGesture(target_bounds);

  // This needs to happen after calling UpdateTargetBoundsForGesture(), because
  // that can change the size of the shelf.
  target_bounds->shelf_bounds_in_shelf = SelectValueForShelfAlignment(
      gfx::Rect(0, 0, shelf_width - status_size.width(),
                target_bounds->shelf_bounds_in_root.height()),
      gfx::Rect(0, 0, target_bounds->shelf_bounds_in_root.width(),
                shelf_height - status_size.height()),
      gfx::Rect(0, 0, target_bounds->shelf_bounds_in_root.width(),
                shelf_height - status_size.height()));

  available_bounds.Subtract(target_bounds->shelf_bounds_in_root);
  available_bounds.Subtract(keyboard_bounds_);

  aura::Window* root = shelf_window->GetRootWindow();
  ::wm::ConvertRectToScreen(root, &available_bounds);
  user_work_area_bounds_ = available_bounds;
}

void ShelfLayoutManager::UpdateTargetBoundsForGesture(
    TargetBounds* target_bounds) const {
  CHECK_EQ(GESTURE_DRAG_IN_PROGRESS, gesture_drag_status_);
  bool horizontal = shelf_->IsHorizontalAlignment();
  aura::Window* window = shelf_widget_->GetNativeWindow();
  gfx::Rect available_bounds = screen_util::GetDisplayBoundsWithShelf(window);
  int resistance_free_region = 0;

  if (gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_HIDDEN &&
      visibility_state() == SHELF_AUTO_HIDE &&
      auto_hide_state() != SHELF_AUTO_HIDE_SHOWN) {
    // If the shelf was hidden when the drag started (and the state hasn't
    // changed since then, e.g. because the tray-menu was shown because of the
    // drag), then allow the drag some resistance-free region at first to make
    // sure the shelf sticks with the finger until the shelf is visible.
    resistance_free_region = kShelfSize - kShelfAutoHideSize;
  }

  bool resist = SelectValueForShelfAlignment(
      gesture_drag_amount_<-resistance_free_region, gesture_drag_amount_>
          resistance_free_region,
      gesture_drag_amount_ < -resistance_free_region);

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
    shelf_height = std::max(shelf_height, 0);
    target_bounds->shelf_bounds_in_root.set_height(shelf_height);
    if (shelf_->IsHorizontalAlignment()) {
      target_bounds->shelf_bounds_in_root.set_y(available_bounds.bottom() -
                                                shelf_height);
    }

    target_bounds->status_bounds_in_shelf.set_y(0);
  } else {
    // Move and size the shelf with the gesture.
    int shelf_width = target_bounds->shelf_bounds_in_root.width();
    bool right_aligned = shelf_->alignment() == SHELF_ALIGNMENT_RIGHT;
    if (right_aligned)
      shelf_width -= translate;
    else
      shelf_width += translate;
    shelf_width = std::max(shelf_width, 0);
    target_bounds->shelf_bounds_in_root.set_width(shelf_width);
    if (right_aligned) {
      target_bounds->shelf_bounds_in_root.set_x(available_bounds.right() -
                                                shelf_width);
    }

    if (right_aligned) {
      target_bounds->status_bounds_in_shelf.set_x(0);
    } else {
      target_bounds->status_bounds_in_shelf.set_x(
          target_bounds->shelf_bounds_in_root.width() - kShelfSize);
    }
  }
}

void ShelfLayoutManager::MaybeUpdateShelfBackground(AnimationChangeType type) {
  const ShelfBackgroundType new_background_type(GetShelfBackgroundType());

  if (new_background_type == shelf_background_type_)
    return;

  shelf_background_type_ = new_background_type;
  for (auto& observer : observers_)
    observer.OnBackgroundUpdated(shelf_background_type_, type);
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
  gfx::Rect shelf_bounds_in_screen = shelf_widget_->GetWindowBoundsInScreen();
  gfx::Vector2d offset = SelectValueForShelfAlignment(
      gfx::Vector2d(0, shelf_bounds_in_screen.height()),
      gfx::Vector2d(-kMaxAutoHideShowShelfRegionSize, 0),
      gfx::Vector2d(shelf_bounds_in_screen.width(), 0));

  gfx::Rect show_shelf_region_in_screen = shelf_bounds_in_screen;
  show_shelf_region_in_screen += offset;
  if (shelf_->IsHorizontalAlignment())
    show_shelf_region_in_screen.set_height(kMaxAutoHideShowShelfRegionSize);
  else
    show_shelf_region_in_screen.set_width(kMaxAutoHideShowShelfRegionSize);

  // TODO: Figure out if we need any special handling when the keyboard is
  // visible.
  return show_shelf_region_in_screen;
}

bool ShelfLayoutManager::HasVisibleWindow() const {
  aura::Window* root = shelf_widget_->GetNativeWindow()->GetRootWindow();
  const aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal();
  // Process the window list and check if there are any visible windows.
  // Ignore app list windows that may be animating to hide after dismissal.
  for (auto* window : windows) {
    if (window->IsVisible() && !IsAppListWindow(window) &&
        root->Contains(window)) {
      return true;
    }
  }
  return false;
}

ShelfAutoHideState ShelfLayoutManager::CalculateAutoHideState(
    ShelfVisibilityState visibility_state) const {
  // Shelf is not available before login.
  // TODO(crbug.com/701157): Remove this when the login webui fake-shelf is
  // replaced with views.
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return SHELF_AUTO_HIDE_HIDDEN;

  if (visibility_state != SHELF_AUTO_HIDE)
    return SHELF_AUTO_HIDE_HIDDEN;

  if (shelf_widget_->IsShowingAppList())
    return SHELF_AUTO_HIDE_SHOWN;

  Sidebar* sidebar =
      RootWindowController::ForWindow(shelf_widget_->GetNativeView())
          ->sidebar();
  if (sidebar && sidebar->IsVisible())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->status_area_widget() &&
      shelf_widget_->status_area_widget()->ShouldShowShelf())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->IsShowingContextMenu())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->IsShowingOverflowBubble())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->IsActive() ||
      (shelf_widget_->status_area_widget() &&
       shelf_widget_->status_area_widget()->IsActive())) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  // If there are no visible windows do not hide the shelf.
  if (!HasVisibleWindow())
    return SHELF_AUTO_HIDE_SHOWN;

  if (gesture_drag_status_ == GESTURE_DRAG_COMPLETE_IN_PROGRESS)
    return gesture_drag_auto_hide_state_;

  // Don't show if the user is dragging the mouse.
  if (in_mouse_drag_)
    return SHELF_AUTO_HIDE_HIDDEN;

  gfx::Rect shelf_region = shelf_widget_->GetWindowBoundsInScreen();
  if (shelf_widget_->status_area_widget() &&
      shelf_widget_->status_area_widget()->IsMessageBubbleShown() &&
      IsVisible()) {
    // Increase the the hit test area to prevent the shelf from disappearing
    // when the mouse is over the bubble gap.
    ShelfAlignment alignment = shelf_->alignment();
    shelf_region.Inset(
        alignment == SHELF_ALIGNMENT_RIGHT ? -kNotificationBubbleGapHeight : 0,
        shelf_->IsHorizontalAlignment() ? -kNotificationBubbleGapHeight : 0,
        alignment == SHELF_ALIGNMENT_LEFT ? -kNotificationBubbleGapHeight : 0,
        0);
  }

  gfx::Point cursor_position_in_screen =
      display::Screen::GetScreen()->GetCursorScreenPoint();
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
  const aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  return shelf_window && shelf_window->Contains(window);
}

bool ShelfLayoutManager::IsStatusAreaWindow(aura::Window* window) {
  if (!window)
    return false;
  const aura::Window* status_window =
      shelf_widget_->status_area_widget()->GetNativeWindow();
  return status_window && status_window->Contains(window);
}

int ShelfLayoutManager::GetWorkAreaInsets(const State& state, int size) const {
  if (state.visibility_state == SHELF_VISIBLE)
    return size;
  return 0;
}

void ShelfLayoutManager::OnLockStateEvent(LockStateObserver::EventType event) {
  if (event == EVENT_LOCK_ANIMATION_STARTED) {
    // Enter the screen locked state and update the visibility to avoid an odd
    // animation when transitioning the orientation from L/R to bottom.
    state_.pre_lock_screen_animation_active = true;
    UpdateShelfVisibilityAfterLoginUIChange();
  } else {
    state_.pre_lock_screen_animation_active = false;
  }
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Check transition changes to/from the add user to session and change the
  // shelf alignment accordingly
  const bool was_adding_user = state_.IsAddingSecondaryUser();
  const bool was_locked = state_.IsScreenLocked();
  state_.session_state = state;
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
  if (was_adding_user != state_.IsAddingSecondaryUser()) {
    UpdateShelfVisibilityAfterLoginUIChange();
    return;
  }

  // Force the shelf to layout for alignment (bottom if locked, restore the
  // previous alignment otherwise).
  if (was_locked != state_.IsScreenLocked())
    UpdateShelfVisibilityAfterLoginUIChange();

  TargetBounds target_bounds;
  CalculateTargetBounds(state_, &target_bounds);
  UpdateBoundsAndOpacity(target_bounds, true /* animate */,
                         true /* change_work_area */, nullptr);
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnLoginStatusChanged(LoginStatus loing_status) {
  UpdateVisibilityState();
}

void ShelfLayoutManager::UpdateShelfVisibilityAfterLoginUIChange() {
  UpdateVisibilityState();
  LayoutShelf();
}

float ShelfLayoutManager::ComputeTargetOpacity(const State& state) {
  if (gesture_drag_status_ == GESTURE_DRAG_IN_PROGRESS ||
      state.visibility_state == SHELF_VISIBLE) {
    return 1.0f;
  }
  // In Chrome OS Material Design, when shelf is hidden during auto hide state,
  // target bounds are also hidden. So the window can extend to the edge of
  // screen.
  return (state.visibility_state == SHELF_AUTO_HIDE &&
          state.auto_hide_state == SHELF_AUTO_HIDE_SHOWN)
             ? 1.0f
             : 0.0f;
}

bool ShelfLayoutManager::IsShelfHiddenForFullscreen() const {
  const aura::Window* fullscreen_window =
      wm::GetWindowForFullscreenMode(shelf_widget_->GetNativeWindow());
  return fullscreen_window &&
         wm::GetWindowState(fullscreen_window)->GetHideShelfWhenFullscreen();
}

bool ShelfLayoutManager::IsShelfAutoHideForFullscreenMaximized() const {
  wm::WindowState* active_window = wm::GetActiveWindowState();
  return active_window &&
         active_window->autohide_shelf_when_maximized_or_fullscreen();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, Gesture functions:

void ShelfLayoutManager::StartGestureDrag(
    const ui::GestureEvent& gesture_in_screen) {
  if (CanStartFullscreenAppListDrag(
          gesture_in_screen.details().scroll_y_hint())) {
    const gfx::Rect shelf_bounds = GetIdealBounds();
    shelf_background_type_before_drag_ = shelf_background_type_;
    gesture_drag_status_ = GESTURE_DRAG_APPLIST_IN_PROGRESS;
    Shell::Get()->app_list()->Show(
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(shelf_widget_->GetNativeWindow())
            .id(),
        app_list::kSwipeFromShelf);
    Shell::Get()->app_list()->UpdateYPositionAndOpacity(
        shelf_bounds.y(), GetAppListBackgroundOpacityOnShelfOpacity());
    launcher_above_shelf_bottom_amount_ =
        shelf_bounds.bottom() - gesture_in_screen.location().y();
  } else {
    // Disable the shelf dragging if the fullscreen app list is opened.
    if (is_app_list_visible_) {
      return;
    }
    gesture_drag_status_ = GESTURE_DRAG_IN_PROGRESS;
    gesture_drag_auto_hide_state_ = visibility_state() == SHELF_AUTO_HIDE
                                        ? auto_hide_state()
                                        : SHELF_AUTO_HIDE_SHOWN;
    MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
    gesture_drag_amount_ = 0.f;
  }
}

void ShelfLayoutManager::UpdateGestureDrag(
    const ui::GestureEvent& gesture_in_screen) {
  if (gesture_drag_status_ == GESTURE_DRAG_APPLIST_IN_PROGRESS) {
    // Dismiss the app list if the shelf changed to vertical alignment during
    // dragging.
    if (!shelf_->IsHorizontalAlignment()) {
      Shell::Get()->app_list()->Dismiss();
      launcher_above_shelf_bottom_amount_ = 0.f;
      gesture_drag_status_ = GESTURE_DRAG_NONE;
      return;
    }
    const gfx::Rect shelf_bounds = GetIdealBounds();
    Shell::Get()->app_list()->UpdateYPositionAndOpacity(
        std::min(gesture_in_screen.location().y(), shelf_bounds.y()),
        GetAppListBackgroundOpacityOnShelfOpacity());
    launcher_above_shelf_bottom_amount_ =
        shelf_bounds.bottom() - gesture_in_screen.location().y();
  } else {
    gesture_drag_amount_ +=
        PrimaryAxisValue(gesture_in_screen.details().scroll_y(),
                         gesture_in_screen.details().scroll_x());
    LayoutShelf();
  }
}

void ShelfLayoutManager::CompleteGestureDrag(
    const ui::GestureEvent& gesture_in_screen) {
  bool should_change = false;
  if (gesture_in_screen.type() == ui::ET_GESTURE_SCROLL_END) {
    // The visibility of the shelf changes only if the shelf was dragged X%
    // along the correct axis. If the shelf was already visible, then the
    // direction of the drag does not matter.
    const float kDragHideThreshold = 0.4f;
    const gfx::Rect bounds = GetIdealBounds();
    const float drag_ratio =
        fabs(gesture_drag_amount_) /
        (shelf_->IsHorizontalAlignment() ? bounds.height() : bounds.width());

    should_change =
        IsSwipingCorrectDirection() && drag_ratio > kDragHideThreshold;
  } else if (gesture_in_screen.type() == ui::ET_SCROLL_FLING_START) {
    should_change = IsSwipingCorrectDirection();
  } else {
    NOTREACHED();
  }

  if (!should_change) {
    CancelGestureDrag();
    return;
  }

  shelf_widget_->Deactivate();
  shelf_widget_->status_area_widget()->Deactivate();

  gesture_drag_auto_hide_state_ =
      gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN
          ? SHELF_AUTO_HIDE_HIDDEN
          : SHELF_AUTO_HIDE_SHOWN;
  ShelfAutoHideBehavior new_auto_hide_behavior =
      gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN
          ? SHELF_AUTO_HIDE_BEHAVIOR_NEVER
          : SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;

  // When in fullscreen and the shelf is forced to be auto hidden, the auto hide
  // behavior affects neither the visibility state nor the auto hide state. Set
  // |gesture_drag_status_| to GESTURE_DRAG_COMPLETE_IN_PROGRESS to set the auto
  // hide state to |gesture_drag_auto_hide_state_|. Only change the auto-hide
  // behavior if there is at least one window visible.
  gesture_drag_status_ = GESTURE_DRAG_COMPLETE_IN_PROGRESS;
  if (shelf_->auto_hide_behavior() != new_auto_hide_behavior &&
      HasVisibleWindow()) {
    shelf_->SetAutoHideBehavior(new_auto_hide_behavior);
  } else {
    UpdateVisibilityState();
  }
  gesture_drag_status_ = GESTURE_DRAG_NONE;
}

void ShelfLayoutManager::CompleteAppListDrag(
    const ui::GestureEvent& gesture_in_screen) {
  // Change the shelf alignment to vertical during drag will reset
  // |gesture_drag_status_| to |GESTURE_DRAG_NONE|.
  if (gesture_drag_status_ == GESTURE_DRAG_NONE)
    return;

  using app_list::mojom::AppListState;
  AppListState app_list_state = AppListState::PEEKING;
  if (gesture_in_screen.type() == ui::ET_SCROLL_FLING_START &&
      fabs(gesture_in_screen.details().velocity_y()) >
          kAppListDragVelocityThreshold) {
    // If the scroll sequence terminates with a fling, show the fullscreen app
    // list if the fling was fast enough and in the correct direction, otherwise
    // close it.
    app_list_state = gesture_in_screen.details().velocity_y() < 0
                         ? AppListState::FULLSCREEN_ALL_APPS
                         : AppListState::CLOSED;
  } else {
    // Snap the app list to corresponding state according to the snapping
    // thresholds.
    if (Shell::Get()
            ->tablet_mode_controller()
            ->IsTabletModeWindowManagerEnabled()) {
      app_list_state = launcher_above_shelf_bottom_amount_ >
                               kAppListDragSnapToFullscreenThreshold
                           ? AppListState::FULLSCREEN_ALL_APPS
                           : AppListState::CLOSED;
    } else {
      if (launcher_above_shelf_bottom_amount_ <=
          kAppListDragSnapToClosedThreshold)
        app_list_state = AppListState::CLOSED;
      else if (launcher_above_shelf_bottom_amount_ <=
               kAppListDragSnapToPeekingThreshold)
        app_list_state = AppListState::PEEKING;
      else
        app_list_state = AppListState::FULLSCREEN_ALL_APPS;
    }
  }

  Shell::Get()->app_list()->EndDragFromShelf(app_list_state);

  gesture_drag_status_ = GESTURE_DRAG_NONE;
}

void ShelfLayoutManager::CancelGestureDrag() {
  if (gesture_drag_status_ == GESTURE_DRAG_APPLIST_IN_PROGRESS) {
    Shell::Get()->app_list()->Dismiss();
  } else {
    gesture_drag_status_ = GESTURE_DRAG_CANCEL_IN_PROGRESS;
    UpdateVisibilityState();
  }
  gesture_drag_status_ = GESTURE_DRAG_NONE;
}

bool ShelfLayoutManager::CanStartFullscreenAppListDrag(
    float scroll_y_hint) const {
  // Fullscreen app list can only be dragged from bottom alignment shelf.
  if (!shelf_->IsHorizontalAlignment())
    return false;

  // If the shelf is not visible, swiping up should show the shelf.
  if (!IsVisible())
    return false;

  // If app list is already opened, swiping up on the shelf should keep the app
  // list opened.
  if (is_app_list_visible_)
    return false;

  // Swipes down on shelf should hide the shelf.
  if (scroll_y_hint >= 0)
    return false;

  return true;
}

float ShelfLayoutManager::GetAppListBackgroundOpacityOnShelfOpacity() {
  float shelf_opacity = shelf_widget_->GetBackgroundAlphaValue(
                            shelf_background_type_before_drag_) /
                        static_cast<float>(ShelfBackgroundAnimator::kMaxAlpha);
  if (launcher_above_shelf_bottom_amount_ < kShelfSize)
    return shelf_opacity;
  float launcher_above_shelf_amount =
      std::max(0.f, launcher_above_shelf_bottom_amount_ - kShelfSize);
  float coefficient =
      std::min(launcher_above_shelf_amount /
                   (app_list::AppListView::kNumOfShelfSize * kShelfSize),
               1.0f);
  float app_list_view_opacity =
      is_background_blur_enabled_
          ? app_list::AppListView::kAppListOpacityWithBlur
          : app_list::AppListView::kAppListOpacity;
  return app_list_view_opacity * coefficient +
         (1 - coefficient) * shelf_opacity;
}

bool ShelfLayoutManager::IsSwipingCorrectDirection() {
  switch (shelf_->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
    case SHELF_ALIGNMENT_RIGHT:
      if (gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN)
        return gesture_drag_amount_ > 0;
      return gesture_drag_amount_ < 0;
    case SHELF_ALIGNMENT_LEFT:
      if (gesture_drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN)
        return gesture_drag_amount_ < 0;
      return gesture_drag_amount_ > 0;
  }
  return false;
}

}  // namespace ash
