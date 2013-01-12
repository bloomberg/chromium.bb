// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SHELF_LAYOUT_MANAGER_H_
#define ASH_WM_SHELF_LAYOUT_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/launcher/launcher.h"
#include "ash/shelf_types.h"
#include "ash/shell_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/timer.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/layout_manager.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindow;
}

namespace ui {
class GestureEvent;
}

namespace ash {
class ScreenAsh;
namespace internal {

class ShelfLayoutManagerTest;
class StatusAreaWidget;
class WorkspaceController;

// ShelfLayoutManager is the layout manager responsible for the launcher and
// status widgets. The launcher is given the total available width and told the
// width of the status area. This allows the launcher to draw the background and
// layout to the status area.
// To respond to bounds changes in the status area StatusAreaLayoutManager works
// closely with ShelfLayoutManager.
class ASH_EXPORT ShelfLayoutManager :
    public aura::LayoutManager,
    public ash::ShellObserver,
    public aura::client::ActivationChangeObserver {
 public:
  class ASH_EXPORT Observer {
   public:
    // Called when the target ShelfLayoutManager will be deleted.
    virtual void WillDeleteShelf() {}

    // Called when the visibility change is scheduled.
    virtual void WillChangeVisibilityState(ShelfVisibilityState new_state) {}

    // Called when the auto hide state is changed.
    virtual void OnAutoHideStateChanged(ShelfAutoHideState new_state) {}
  };

  // We reserve a small area at the bottom of the workspace area to ensure that
  // the bottom-of-window resize handle can be hit.
  static const int kWorkspaceAreaBottomInset;

  // Size of the shelf when auto-hidden.
  static const int kAutoHideSize;

  explicit ShelfLayoutManager(StatusAreaWidget* status);
  virtual ~ShelfLayoutManager();

  // Sets the ShelfAutoHideBehavior. See enum description for details.
  void SetAutoHideBehavior(ShelfAutoHideBehavior behavior);
  ShelfAutoHideBehavior auto_hide_behavior() const {
    return auto_hide_behavior_;
  }

  // Sets the alignment. Returns true if the alignment is changed. Otherwise,
  // returns false.
  bool SetAlignment(ShelfAlignment alignment);
  ShelfAlignment GetAlignment() const { return alignment_; }

  void set_workspace_controller(WorkspaceController* controller) {
    workspace_controller_ = controller;
  }

  views::Widget* launcher_widget() {
    return launcher_ ? launcher_->widget() : NULL;
  }
  const views::Widget* launcher_widget() const {
    return launcher_ ? launcher_->widget() : NULL;
  }
  StatusAreaWidget* status_area_widget() { return status_area_widget_; }

  bool in_layout() const { return in_layout_; }

  // Returns whether the shelf and its contents (launcher, status) are visible
  // on the screen.
  bool IsVisible() const;

  // The launcher is typically created after the layout manager.
  void SetLauncher(Launcher* launcher);
  Launcher* launcher() { return launcher_; }

  // Returns the ideal bounds of the shelf assuming it is visible.
  gfx::Rect GetIdealBounds();

  // Stops any animations and sets the bounds of the launcher and status
  // widgets.
  void LayoutShelf();

  // Updates the visibility state.
  void UpdateVisibilityState();

  // Invoked by the shelf/launcher when the auto-hide state may have changed.
  void UpdateAutoHideState();

  ShelfVisibilityState visibility_state() const {
    return state_.visibility_state;
  }
  ShelfAutoHideState auto_hide_state() const { return state_.auto_hide_state; }

  // Sets whether any windows overlap the shelf. If a window overlaps the shelf
  // the shelf renders slightly differently.
  void SetWindowOverlapsShelf(bool value);
  bool window_overlaps_shelf() const { return window_overlaps_shelf_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Gesture dragging related functions:
  void StartGestureDrag(const ui::GestureEvent& gesture);
  enum DragState {
    DRAG_SHELF,
    DRAG_TRAY
  };
  // Returns DRAG_SHELF if the gesture should continue to drag the entire shelf.
  // Returns DRAG_TRAY if the gesture can start dragging the tray-bubble from
  // this point on.
  DragState UpdateGestureDrag(const ui::GestureEvent& gesture);
  void CompleteGestureDrag(const ui::GestureEvent& gesture);
  void CancelGestureDrag();

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // Overridden from ash::ShellObserver:
  virtual void OnLockStateChanged(bool locked) OVERRIDE;

  // Overriden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  // TODO(harrym|oshima): These templates will be moved to
  // new Shelf class.
  // A helper function that provides a shortcut for choosing
  // values specific to a shelf alignment.
  template<typename T>
  T SelectValueForShelfAlignment(T bottom, T left, T right, T top) const {
    switch (alignment_) {
      case SHELF_ALIGNMENT_BOTTOM:
        return bottom;
      case SHELF_ALIGNMENT_LEFT:
        return left;
      case SHELF_ALIGNMENT_RIGHT:
        return right;
      case SHELF_ALIGNMENT_TOP:
        return top;
    }
    NOTREACHED();
    return right;
  }

  template<typename T>
  T PrimaryAxisValue(T horizontal, T vertical) const {
    return IsHorizontalAlignment() ? horizontal : vertical;
  }

  // Is the shelf's alignment horizontal?
  bool IsHorizontalAlignment() const;

  // Returns a ShelfLayoutManager on the display which has a launcher for
  // given |window|. See RootWindowController::ForLauncher for more info.
  static ShelfLayoutManager* ForLauncher(aura::Window* window);

 private:
  class AutoHideEventFilter;
  class UpdateShelfObserver;
  friend class ash::ScreenAsh;
  friend class ShelfLayoutManagerTest;

  struct TargetBounds {
    TargetBounds();

    float opacity;
    gfx::Rect launcher_bounds_in_root;
    gfx::Rect status_bounds_in_root;
    gfx::Insets work_area_insets;
  };

  struct State {
    State() : visibility_state(SHELF_VISIBLE),
              auto_hide_state(SHELF_AUTO_HIDE_HIDDEN),
              is_screen_locked(false) {}

    // Returns true if the two states are considered equal. As
    // |auto_hide_state| only matters if |visibility_state| is
    // |SHELF_AUTO_HIDE|, Equals() ignores the |auto_hide_state| as
    // appropriate.
    bool Equals(const State& other) const {
      return other.visibility_state == visibility_state &&
          (visibility_state != SHELF_AUTO_HIDE ||
           other.auto_hide_state == auto_hide_state) &&
          other.is_screen_locked == is_screen_locked;
    }

    ShelfVisibilityState visibility_state;
    ShelfAutoHideState auto_hide_state;
    bool is_screen_locked;
  };

  // Sets the visibility of the shelf to |state|.
  void SetState(ShelfVisibilityState visibility_state);

  // Stops any animations.
  void StopAnimating();

  // Returns the width (if aligned to the side) or height (if aligned to the
  // bottom).
  void GetShelfSize(int* width, int* height);

  // Insets |bounds| by |inset| on the edge the shelf is aligned to.
  void AdjustBoundsBasedOnAlignment(int inset, gfx::Rect* bounds) const;

  // Calculates the target bounds assuming visibility of |visible|.
  void CalculateTargetBounds(const State& state, TargetBounds* target_bounds);

  // Updates the target bounds if a gesture-drag is in progress. This is only
  // used by |CalculateTargetBounds()|.
  void UpdateTargetBoundsForGesture(TargetBounds* target_bounds) const;

  // Updates the background of the shelf.
  void UpdateShelfBackground(BackgroundAnimator::ChangeType type);

  // Returns whether the launcher should draw a background.
  bool GetLauncherPaintsBackground() const;

  // Updates the auto hide state immediately.
  void UpdateAutoHideStateNow();

  // Returns the AutoHideState. This value is determined from the launcher and
  // tray.
  ShelfAutoHideState CalculateAutoHideState(
      ShelfVisibilityState visibility_state) const;

  // Updates the hit test bounds override for launcher and status area.
  void UpdateHitTestBounds();

  // Returns true if |window| is a descendant of the shelf.
  bool IsShelfWindow(aura::Window* window);

  int GetWorkAreaSize(const State& state, int size) const;

  // The RootWindow is cached so that we don't invoke Shell::GetInstance() from
  // our destructor. We avoid that as at the time we're deleted Shell is being
  // deleted too.
  aura::RootWindow* root_window_;

  // True when inside LayoutShelf method. Used to prevent calling LayoutShelf
  // again from SetChildBounds().
  bool in_layout_;

  // See description above setter.
  ShelfAutoHideBehavior auto_hide_behavior_;

  ShelfAlignment alignment_;

  // Current state.
  State state_;

  Launcher* launcher_;
  StatusAreaWidget* status_area_widget_;

  WorkspaceController* workspace_controller_;

  // Do any windows overlap the shelf? This is maintained by WorkspaceManager.
  bool window_overlaps_shelf_;

  base::OneShotTimer<ShelfLayoutManager> auto_hide_timer_;

  // EventFilter used to detect when user moves the mouse over the launcher to
  // trigger showing the launcher.
  scoped_ptr<AutoHideEventFilter> event_filter_;

  ObserverList<Observer> observers_;

  // The shelf reacts to gesture-drags, and can be set to auto-hide for certain
  // gestures. Some shelf behaviour (e.g. visibility state, background color
  // etc.) are affected by various stages of the drag. The enum keeps track of
  // the present status of the gesture drag.
  enum GestureDragStatus {
    GESTURE_DRAG_NONE,
    GESTURE_DRAG_IN_PROGRESS,
    GESTURE_DRAG_COMPLETE_IN_PROGRESS
  };
  GestureDragStatus gesture_drag_status_;

  // Tracks the amount of the drag. The value is only valid when
  // |gesture_drag_status_| is set to GESTURE_DRAG_IN_PROGRESS.
  float gesture_drag_amount_;

  // Manage the auto-hide state during the gesture.
  ShelfAutoHideState gesture_drag_auto_hide_state_;

  // Used to delay updating shelf background.
  UpdateShelfObserver* update_shelf_observer_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SHELF_LAYOUT_MANAGER_H_
