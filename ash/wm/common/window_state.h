// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WINDOW_STATE_H_
#define ASH_WM_COMMON_WINDOW_STATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/common/drag_details.h"
#include "ash/wm/common/wm_types.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {
class LockWindowState;
class MaximizeModeWindowState;

namespace wm {
class WindowStateDelegate;
class WindowStateObserver;
class WMEvent;
class WmWindow;

// WindowState manages and defines ash specific window state and
// behavior. Ash specific per-window state (such as ones that controls
// window manager behavior) and ash specific window behavior (such as
// maximize, minimize, snap sizing etc) should be added here instead
// of defining separate functions (like |MaximizeWindow(aura::Window*
// window)|) or using aura Window property.
// The WindowState gets created when first accessed by
// |wm::GetWindowState|, and deleted when the window is deleted.
// Prefer using this class instead of passing aura::Window* around in
// ash code as this is often what you need to interact with, and
// accessing the window using |window()| is cheap.
class ASH_EXPORT WindowState {
 public:
  // A subclass of State class represents one of the window's states
  // that corresponds to WindowStateType in Ash environment, e.g.
  // maximized, minimized or side snapped, as subclass.
  // Each subclass defines its own behavior and transition for each WMEvent.
  class State {
   public:
    State() {}
    virtual ~State() {}

    // Update WindowState based on |event|.
    virtual void OnWMEvent(WindowState* window_state, const WMEvent* event) = 0;

    virtual WindowStateType GetType() const = 0;

    // Gets called when the state object became active and the managed window
    // needs to be adjusted to the State's requirement.
    // The passed |previous_state| may be used to properly implement state
    // transitions such as bound animations from the previous state.
    // Note: This only gets called when the state object gets changed.
    virtual void AttachState(WindowState* window_state,
                             State* previous_state) = 0;

    // Gets called before the state objects gets deactivated / detached from the
    // window, so that it can save the various states it is interested in.
    // Note: This only gets called when the state object gets changed.
    virtual void DetachState(WindowState* window_state) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(State);
  };

  // Call GetWindowState() to instantiate this class.
  ~WindowState();

  WmWindow* window() { return window_; }
  const WmWindow* window() const { return window_; }

  bool HasDelegate() const;
  void SetDelegate(std::unique_ptr<WindowStateDelegate> delegate);

  // Returns the window's current ash state type.
  // Refer to WindowStateType definition in wm_types.h as for why Ash
  // has its own state type.
  WindowStateType GetStateType() const;

  // Predicates to check window state.
  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsFullscreen() const;
  bool IsMaximizedOrFullscreen() const;
  bool IsSnapped() const;

  // True if the window's state type is WINDOW_STATE_TYPE_NORMAL or
  // WINDOW_STATE_TYPE_DEFAULT.
  bool IsNormalStateType() const;

  bool IsNormalOrSnapped() const;

  bool IsActive() const;
  bool IsDocked() const;

  // Returns true if the window's location can be controlled by the user.
  bool IsUserPositionable() const;

  // Checks if the window can change its state accordingly.
  bool CanMaximize() const;
  bool CanMinimize() const;
  bool CanResize() const;
  bool CanSnap() const;
  bool CanActivate() const;

  // Returns true if the window has restore bounds.
  bool HasRestoreBounds() const;

  // These methods use aura::WindowProperty to change the window's state
  // instead of using WMEvent directly. This is to use the same mechanism as
  // what views::Widget is using.
  void Maximize();
  void Minimize();
  void Unminimize();

  void Activate();
  void Deactivate();

  // Set the window state to normal.
  // TODO(oshima): Change to use RESTORE event.
  void Restore();

  // Caches, then disables always on top state and then stacks |window_| below
  // |window_on_top| if a |window_| is currently in always on top state.
  void DisableAlwaysOnTop(WmWindow* window_on_top);

  // Restores always on top state that a window might have cached.
  void RestoreAlwaysOnTop();

  // Invoked when a WMevent occurs, which drives the internal
  // state machine.
  void OnWMEvent(const WMEvent* event);

  // TODO(oshima): Try hiding these methods and making them accessible only to
  // state impl. State changes should happen through events (as much
  // as possible).

  // Saves the current bounds to be used as a restore bounds.
  void SaveCurrentBoundsForRestore();

  // Same as |GetRestoreBoundsInScreen| except that it returns the
  // bounds in the parent's coordinates.
  gfx::Rect GetRestoreBoundsInParent() const;

  // Returns the restore bounds property on the window in the virtual screen
  // coordinates. The bounds can be NULL if the bounds property does not
  // exist for the window. The window owns the bounds object.
  gfx::Rect GetRestoreBoundsInScreen() const;

  // Same as |SetRestoreBoundsInScreen| except that the bounds is in the
  // parent's coordinates.
  void SetRestoreBoundsInParent(const gfx::Rect& bounds_in_parent);

  // Sets the restore bounds property on the window in the virtual screen
  // coordinates.  Deletes existing bounds value if exists.
  void SetRestoreBoundsInScreen(const gfx::Rect& bounds_in_screen);

  // Deletes and clears the restore bounds property on the window.
  void ClearRestoreBounds();

  // Replace the State object of a window with a state handler which can
  // implement a new window manager type. The passed object will be owned
  // by this object and the returned object will be owned by the caller.
  std::unique_ptr<State> SetStateObject(std::unique_ptr<State> new_state);

  // True if the window should be unminimized to the restore bounds, as
  // opposed to the window's current bounds. |unminimized_to_restore_bounds_| is
  // reset to the default value after the window is unminimized.
  bool unminimize_to_restore_bounds() const {
    return unminimize_to_restore_bounds_;
  }
  void set_unminimize_to_restore_bounds(bool value) {
    unminimize_to_restore_bounds_ = value;
  }

  // Gets/sets whether the shelf should be hidden when this window is
  // fullscreen.
  bool hide_shelf_when_fullscreen() const {
    return hide_shelf_when_fullscreen_;
  }

  void set_hide_shelf_when_fullscreen(bool value) {
    hide_shelf_when_fullscreen_ = value;
  }

  // If the minimum visibility is true, ash will try to keep a
  // minimum amount of the window is always visible on the work area
  // when shown.
  // TODO(oshima): Consolidate this and window_position_managed
  // into single parameter to control the window placement.
  bool minimum_visibility() const { return minimum_visibility_; }
  void set_minimum_visibility(bool minimum_visibility) {
    minimum_visibility_ = minimum_visibility;
  }

  // Specifies if the window can be dragged by the user via the caption or not.
  bool can_be_dragged() const { return can_be_dragged_; }
  void set_can_be_dragged(bool can_be_dragged) {
    can_be_dragged_ = can_be_dragged;
  }

  // Gets/Sets the bounds of the window before it was moved by the auto window
  // management. As long as it was not auto-managed, it will return NULL.
  const gfx::Rect* pre_auto_manage_window_bounds() const {
    return pre_auto_manage_window_bounds_.get();
  }
  void SetPreAutoManageWindowBounds(const gfx::Rect& bounds);

  // Layout related properties

  void AddObserver(WindowStateObserver* observer);
  void RemoveObserver(WindowStateObserver* observer);

  // Whether the window is being dragged.
  bool is_dragged() const { return !!drag_details_; }

  // Whether or not the window's position can be managed by the
  // auto management logic.
  bool window_position_managed() const { return window_position_managed_; }
  void set_window_position_managed(bool window_position_managed) {
    window_position_managed_ = window_position_managed;
  }

  // Whether or not the window's position or size was changed by a user.
  bool bounds_changed_by_user() const { return bounds_changed_by_user_; }
  void set_bounds_changed_by_user(bool bounds_changed_by_user);

  // True if this window is an attached panel.
  bool panel_attached() const { return panel_attached_; }
  void set_panel_attached(bool panel_attached) {
    panel_attached_ = panel_attached;
  }

  // True if the window is ignored by the shelf layout manager for
  // purposes of darkening the shelf.
  bool ignored_by_shelf() const { return ignored_by_shelf_; }
  void set_ignored_by_shelf(bool ignored_by_shelf) {
    ignored_by_shelf_ = ignored_by_shelf;
  }

  // True if the window should be offered a chance to consume special system
  // keys such as brightness, volume, etc. that are usually handled by the
  // shell.
  bool can_consume_system_keys() const { return can_consume_system_keys_; }
  void set_can_consume_system_keys(bool can_consume_system_keys) {
    can_consume_system_keys_ = can_consume_system_keys;
  }

  // True if this window has requested that the top-row keys (back, forward,
  // brightness, volume) should be treated as function keys.
  bool top_row_keys_are_function_keys() const {
    return top_row_keys_are_function_keys_;
  }
  void set_top_row_keys_are_function_keys(bool value) {
    top_row_keys_are_function_keys_ = value;
  }

  // True if the window is in "immersive full screen mode" which is slightly
  // different from the normal fullscreen mode by allowing the user to reveal
  // the top portion of the window through a touch / mouse gesture. It might
  // also allow the shelf to be shown in some situations.
  bool in_immersive_fullscreen() const { return in_immersive_fullscreen_; }
  void set_in_immersive_fullscreen(bool enable) {
    in_immersive_fullscreen_ = enable;
  }

  // Creates and takes ownership of a pointer to DragDetails when resizing is
  // active. This should be done before a resizer gets created.
  void CreateDragDetails(const gfx::Point& point_in_parent,
                         int window_component,
                         aura::client::WindowMoveSource source);

  // Deletes and clears a pointer to DragDetails. This should be done when the
  // resizer gets destroyed.
  void DeleteDragDetails();

  // Sets the currently stored restore bounds and clears the restore bounds.
  void SetAndClearRestoreBounds();

  // Returns a pointer to DragDetails during drag operations.
  const DragDetails* drag_details() const { return drag_details_.get(); }
  DragDetails* drag_details() { return drag_details_.get(); }

  // Called from the associated WmWindow once the show state changes.
  void OnWindowShowStateChanged();

 private:
  friend class DefaultState;
  friend class ash::LockWindowState;
  friend class ash::MaximizeModeWindowState;
  friend ASH_EXPORT WindowState* GetWindowState(aura::Window*);
  FRIEND_TEST_ALL_PREFIXES(WindowAnimationsTest, CrossFadeToBounds);
  FRIEND_TEST_ALL_PREFIXES(WindowAnimationsTest,
                           CrossFadeToBoundsFromTransform);

  explicit WindowState(WmWindow* window);

  WindowStateDelegate* delegate() { return delegate_.get(); }

  // Returns the window's current always_on_top state.
  bool GetAlwaysOnTop() const;

  // Returns the window's current show state.
  ui::WindowShowState GetShowState() const;

  // Sets the window's bounds in screen coordinates.
  void SetBoundsInScreen(const gfx::Rect& bounds_in_screen);

  // Adjusts the |bounds| so that they are flush with the edge of the
  // workspace if the window represented by |window_state| is side snapped.
  void AdjustSnappedBounds(gfx::Rect* bounds);

  // Updates the window show state according to the current window state type.
  // Note that this does not update the window bounds.
  void UpdateWindowShowStateFromStateType();

  void NotifyPreStateTypeChange(WindowStateType old_window_state_type);
  void NotifyPostStateTypeChange(WindowStateType old_window_state_type);

  // Sets |bounds| as is and ensure the layer is aligned with pixel boundary.
  void SetBoundsDirect(const gfx::Rect& bounds);

  // Sets the window's |bounds| with constraint where the size of the
  // new bounds will not exceeds the size of the work area.
  void SetBoundsConstrained(const gfx::Rect& bounds);

  // Sets the wndow's |bounds| and transitions to the new bounds with
  // a scale animation.
  void SetBoundsDirectAnimated(const gfx::Rect& bounds);

  // Sets the window's |bounds| and transition to the new bounds with
  // a cross fade animation.
  void SetBoundsDirectCrossFade(const gfx::Rect& bounds);

  // The owner of this window settings.
  WmWindow* window_;
  std::unique_ptr<WindowStateDelegate> delegate_;

  bool window_position_managed_;
  bool bounds_changed_by_user_;
  bool panel_attached_;
  bool ignored_by_shelf_;
  bool can_consume_system_keys_;
  bool top_row_keys_are_function_keys_;
  std::unique_ptr<DragDetails> drag_details_;

  bool unminimize_to_restore_bounds_;
  bool in_immersive_fullscreen_;
  bool hide_shelf_when_fullscreen_;
  bool minimum_visibility_;
  bool can_be_dragged_;
  bool cached_always_on_top_;

  // A property to remember the window position which was set before the
  // auto window position manager changed the window bounds, so that it can get
  // restored when only this one window gets shown.
  std::unique_ptr<gfx::Rect> pre_auto_manage_window_bounds_;

  base::ObserverList<WindowStateObserver> observer_list_;

  // True to ignore a property change event to avoid reentrance in
  // UpdateWindowStateType()
  bool ignore_property_change_;

  std::unique_ptr<State> current_state_;

  DISALLOW_COPY_AND_ASSIGN(WindowState);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WINDOW_STATE_H_
