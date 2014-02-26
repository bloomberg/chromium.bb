// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_STATE_H_
#define ASH_WM_WINDOW_STATE_H_

#include "ash/ash_export.h"
#include "ash/wm/drag_details.h"
#include "ash/wm/wm_types.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {
namespace internal {
class WorkspaceLayoutManager;
}

namespace wm {
class WindowStateDelegate;
class WindowStateObserver;

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
  // that corresponds WnidowShowType to in Ash environment, e.g.
  // maximized, minimized or side snapped, as subclass.
  // Each subclass defines its own behavior and transition for each WMEvent.
  class State {
   public:
    State() {}
    virtual ~State() {}

    // Update WindowState based on |event|.
    virtual void OnWMEvent(WindowState* state, WMEvent event) = 0;

    // See WindowState::RequestBounds.
    virtual void RequestBounds(WindowState* state,
                               const gfx::Rect& requested_bounds)  = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(State);
  };

  explicit WindowState(aura::Window* window);
  virtual ~WindowState();

  aura::Window* window() { return window_; }
  const aura::Window* window() const { return window_; }

  bool HasDelegate() const;
  void SetDelegate(scoped_ptr<WindowStateDelegate> delegate);

  // Returns the window's current show state.
  ui::WindowShowState GetShowState() const;

  // Returns the window's current ash show type.
  // Refer to WindowShowType definition in wm_types.h as for why Ash
  // has its own show type.
  WindowShowType window_show_type() const { return window_show_type_; }

  // Predicates to check window state.
  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsFullscreen() const;
  bool IsMaximizedOrFullscreen() const;
  bool IsSnapped() const;

  // True if the window's show type is SHOW_TYPE_NORMAL or SHOW_TYPE_DEFAULT.
  bool IsNormalShowType() const;

  bool IsNormalOrSnapped() const;

  bool IsActive() const;
  bool IsDocked() const;

  // Checks if the window can change its state accordingly.
  bool CanMaximize() const;
  bool CanMinimize() const;
  bool CanResize() const;
  bool CanSnap() const;
  bool CanActivate() const;

  // Returns true if the window has restore bounds.
  bool HasRestoreBounds() const;

  void Maximize();
  void Minimize();
  void Unminimize();
  void Activate();
  void Deactivate();
  void Restore();
  void ToggleFullscreen();
  void SnapLeftWithDefaultWidth();
  void SnapRightWithDefaultWidth();

  // A window is requested to be the given bounds. The request may or
  // may not be fulfilled depending on the requested bounds and window's
  // state.
  // TODO(oshima): Convert this to a WMEvent.
  void RequestBounds(const gfx::Rect& bounds);

  // Invoked when a WMevent occurs, which drives the internal
  // state machine.
  void OnWMEvent(WMEvent event);

  // Sets the window's bounds in screen coordinates.
  void SetBoundsInScreen(const gfx::Rect& bounds_in_screen);

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

  // Sets/gets the flag to suppress the cross-fade animation for
  // the transition to the fullscreen state.
  bool animate_to_fullscreen() const {
    return animate_to_fullscreen_;
  }
  void set_animate_to_fullscreen(bool value) {
    animate_to_fullscreen_ = value;
  }

  // If the minimum visibilty is true, ash will try to keep a
  // minimum amount of the window is always visible on the work area
  // when shown.
  // TODO(oshima): Consolidate this and window_position_managed
  // into single parameter to control the window placement.
  bool minimum_visibility() const {
    return minimum_visibility_;
  }
  void set_minimum_visibility(bool minimum_visibility) {
    minimum_visibility_ = minimum_visibility;
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
  bool is_dragged() const {
    return drag_details_;
  }

  // Whether or not the window's position can be managed by the
  // auto management logic.
  bool window_position_managed() const { return window_position_managed_; }
  void set_window_position_managed(bool window_position_managed) {
    window_position_managed_ = window_position_managed;
  }

  // Whether or not the window's position or size was changed by a user.
  bool bounds_changed_by_user() const { return bounds_changed_by_user_; }
  void set_bounds_changed_by_user(bool bounds_changed_by_user) {
    bounds_changed_by_user_ = bounds_changed_by_user;
  }

  // True if this window is an attached panel.
  bool panel_attached() const {
    return panel_attached_;
  }
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

  // Creates and takes ownership of a pointer to DragDetails when resizing is
  // active. This should be done before a resizer gets created.
  void CreateDragDetails(aura::Window* window,
                         const gfx::Point& point_in_parent,
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

  // aura::WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;

 private:
  friend class DefaultState;
  FRIEND_TEST_ALL_PREFIXES(WindowAnimationsTest, CrossFadeToBounds);

  WindowStateDelegate* delegate() { return delegate_.get(); }

  // Adjusts the |bounds| so that they are flush with the edge of the
  // workspace if the window represented by |window_state| is side snapped.
  void AdjustSnappedBounds(gfx::Rect* bounds);

  // Snaps the window left or right with the default width.
  void SnapWindowWithDefaultWidth(WindowShowType left_or_right);

  // Sets the window show type and updates the show state if necessary.
  // Note that this does not update the window bounds.
  void UpdateWindowShowType(WindowShowType new_window_show_type);

  void NotifyPreShowTypeChange(WindowShowType old_window_show_type);
  void NotifyPostShowTypeChange(WindowShowType old_window_show_type);

  // Sets |bounds| as is.
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
  aura::Window* window_;
  scoped_ptr<WindowStateDelegate> delegate_;

  bool window_position_managed_;
  bool bounds_changed_by_user_;
  bool panel_attached_;
  bool ignored_by_shelf_;
  bool can_consume_system_keys_;
  bool top_row_keys_are_function_keys_;
  scoped_ptr<DragDetails> drag_details_;

  bool unminimize_to_restore_bounds_;
  bool hide_shelf_when_fullscreen_;
  bool animate_to_fullscreen_;
  bool minimum_visibility_;

  // A property to remember the window position which was set before the
  // auto window position manager changed the window bounds, so that it can get
  // restored when only this one window gets shown.
  scoped_ptr<gfx::Rect> pre_auto_manage_window_bounds_;

  ObserverList<WindowStateObserver> observer_list_;

  // True to ignore a property change event to avoid reentrance in
  // UpdateWindowShowType()
  bool ignore_property_change_;

  WindowShowType window_show_type_;

  scoped_ptr<State> current_state_;

  DISALLOW_COPY_AND_ASSIGN(WindowState);
};

// Returns the WindowState for active window. Returns |NULL|
// if there is no active window.
ASH_EXPORT WindowState* GetActiveWindowState();

// Returns the WindowState for |window|. Creates WindowState
// if it didn't exist. The settings object is owned by |window|.
ASH_EXPORT WindowState* GetWindowState(aura::Window* window);

// const version of GetWindowState.
ASH_EXPORT const WindowState*
GetWindowState(const aura::Window* window);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_STATE_H_
