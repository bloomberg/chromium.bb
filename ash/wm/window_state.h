// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_STATE_H_
#define ASH_WM_WINDOW_STATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/drag_details.h"
#include "ash/wm/wm_types.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
class Rect;
}

namespace ash {
class LockWindowState;
class TabletModeWindowState;

namespace mojom {
enum class WindowPinType;
}

namespace wm {
class WindowState;
class WindowStateDelegate;
class WindowStateObserver;
class WMEvent;

// Returns the WindowState for the active window, null if there is no active
// window.
ASH_EXPORT WindowState* GetActiveWindowState();

// Returns the WindowState for |window|. Creates WindowState if it doesn't
// exist. The returned value is owned by |window| (you should not delete it).
ASH_EXPORT WindowState* GetWindowState(aura::Window* window);
ASH_EXPORT const WindowState* GetWindowState(const aura::Window* window);

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
class ASH_EXPORT WindowState : public aura::WindowObserver {
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
  ~WindowState() override;

  aura::Window* window() { return window_; }
  const aura::Window* window() const { return window_; }

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
  bool IsSnapped() const;
  bool IsPinned() const;
  bool IsTrustedPinned() const;

  // True if the window's state type is WINDOW_STATE_TYPE_MAXIMIZED,
  // WINDOW_STATE_TYPE_FULLSCREEN or WINDOW_STATE_TYPE_PINNED.
  bool IsMaximizedOrFullscreenOrPinned() const;

  // True if the window's state type is WINDOW_STATE_TYPE_NORMAL or
  // WINDOW_STATE_TYPE_DEFAULT.
  bool IsNormalStateType() const;

  bool IsNormalOrSnapped() const;

  bool IsActive() const;

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
  void DisableAlwaysOnTop(aura::Window* window_on_top);

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

  // Gets/sets whether the shelf should be autohidden when this window is
  // fullscreen or active.
  // Note: if true, this will override the logic controlled by
  // hide_shelf_when_fullscreen.
  bool autohide_shelf_when_maximized_or_fullscreen() const {
    return autohide_shelf_when_maximized_or_fullscreen_;
  }

  void set_autohide_shelf_when_maximized_or_fullscreen(bool value) {
    autohide_shelf_when_maximized_or_fullscreen_ = value;
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

  // True if the window is in "immersive full screen mode" which is slightly
  // different from the normal fullscreen mode by allowing the user to reveal
  // the top portion of the window through a touch / mouse gesture. It might
  // also allow the shelf to be shown in some situations.
  bool in_immersive_fullscreen() const { return in_immersive_fullscreen_; }
  void set_in_immersive_fullscreen(bool enable) {
    in_immersive_fullscreen_ = enable;
  }

  // True if the window should not adjust the window's bounds when
  // virtual keyboard bounds changes.
  // TODO(oshima): This is hack. Replace this with proper
  // implementation based on EnsureCaretNotInRect.
  bool ignore_keyboard_bounds_change() const {
    return ignore_keyboard_bounds_change_;
  }
  void set_ignore_keyboard_bounds_change(bool ignore_keyboard_bounds_change) {
    ignore_keyboard_bounds_change_ = ignore_keyboard_bounds_change;
  }

  // True if the window bounds can be updated directly using SET_BOUNDS event.
  void set_allow_set_bounds_direct(bool value) {
    allow_set_bounds_direct_ = value;
  }
  bool allow_set_bounds_direct() const { return allow_set_bounds_direct_; }

  // Creates and takes ownership of a pointer to DragDetails when resizing is
  // active. This should be done before a resizer gets created.
  void CreateDragDetails(const gfx::Point& point_in_parent,
                         int window_component,
                         ::wm::WindowMoveSource source);

  // Deletes and clears a pointer to DragDetails. This should be done when the
  // resizer gets destroyed.
  void DeleteDragDetails();

  // Sets the currently stored restore bounds and clears the restore bounds.
  void SetAndClearRestoreBounds();

  // Returns a pointer to DragDetails during drag operations.
  const DragDetails* drag_details() const { return drag_details_.get(); }
  DragDetails* drag_details() { return drag_details_.get(); }

 private:
  friend class DefaultState;
  friend class ash::LockWindowState;
  friend class ash::TabletModeWindowState;
  friend WindowState* GetWindowState(aura::Window*);
  FRIEND_TEST_ALL_PREFIXES(WindowAnimationsTest, CrossFadeToBounds);
  FRIEND_TEST_ALL_PREFIXES(WindowAnimationsTest,
                           CrossFadeToBoundsFromTransform);

  explicit WindowState(aura::Window* window);

  WindowStateDelegate* delegate() { return delegate_.get(); }

  // Returns the window's current always_on_top state.
  bool GetAlwaysOnTop() const;

  // Returns the window's current show state.
  ui::WindowShowState GetShowState() const;

  // Return the window's current pin type.
  ash::mojom::WindowPinType GetPinType() const;

  // Sets the window's bounds in screen coordinates.
  void SetBoundsInScreen(const gfx::Rect& bounds_in_screen);

  // Adjusts the |bounds| so that they are flush with the edge of the
  // workspace if the window represented by |window_state| is side snapped.
  void AdjustSnappedBounds(gfx::Rect* bounds);

  // Updates the window properties(show state, pin type) according to the
  // current window state type.
  // Note that this does not update the window bounds.
  void UpdateWindowPropertiesFromStateType();

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

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // The owner of this window settings.
  aura::Window* window_;
  std::unique_ptr<WindowStateDelegate> delegate_;

  bool window_position_managed_;
  bool bounds_changed_by_user_;
  bool ignored_by_shelf_;
  bool can_consume_system_keys_;
  std::unique_ptr<DragDetails> drag_details_;

  bool unminimize_to_restore_bounds_;
  bool in_immersive_fullscreen_;
  bool ignore_keyboard_bounds_change_ = false;
  bool hide_shelf_when_fullscreen_;
  bool autohide_shelf_when_maximized_or_fullscreen_;
  bool minimum_visibility_;
  bool can_be_dragged_;
  bool cached_always_on_top_;
  bool allow_set_bounds_direct_ = false;

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

#endif  // ASH_WM_WINDOW_STATE_H_
