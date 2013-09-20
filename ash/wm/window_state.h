// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_STATE_H_
#define ASH_WM_WINDOW_STATE_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace ash {
namespace wm {

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
  class ASH_EXPORT Observer {
   public:
    // Called when the tracked_by_workspace has changed.
    virtual void OnTrackedByWorkspaceChanged(aura::Window* window,
                                             bool old_value) {}
  };

  static bool IsMaximizedOrFullscreenState(ui::WindowShowState state);

  explicit WindowState(aura::Window* window);
  ~WindowState();

  aura::Window* window() { return window_; }
  const aura::Window* window() const { return window_; }

  // Returns the window's current shows tate.
  ui::WindowShowState GetShowState() const;

  // Predicates to check window state.
  bool IsMinimized() const;
  bool IsMaximized() const;
  bool IsFullscreen() const;
  bool IsMaximizedOrFullscreen() const;
  // True if the window's show state is SHOW_STATE_NORMAL or
  // SHOW_STATE_DEFAULT.
  bool IsNormalShowState() const;
  bool IsActive() const;

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
  void ToggleMaximized();

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

  // Sets whether the window should always be restored to the restore bounds
  // (sometimes the workspace layout manager restores the window to its original
  // bounds instead of the restore bounds. Setting this key overrides that
  // behaviour). The flag is reset to the default value after the window is
  // restored.
  bool always_restores_to_restore_bounds() const {
    return always_restores_to_restore_bounds_;
  }
  void set_always_restores_to_restore_bounds(bool value) {
    always_restores_to_restore_bounds_ = value;
  }

  // Gets/Sets the bounds of the window before it was moved by the auto window
  // management. As long as it was not auto-managed, it will return NULL.
  const gfx::Rect* pre_auto_manage_window_bounds() const {
    return pre_auto_manage_window_bounds_.get();
  }
  void SetPreAutoManageWindowBounds(const gfx::Rect& bounds);

  // Layout related properties

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Whether the window is tracked by workspace code. Default is
  // true. If set to false the workspace does not switch the current
  // workspace, nor does it attempt to impose constraints on the
  // bounds of the window. This is intended for tab dragging.
  bool tracked_by_workspace() const { return tracked_by_workspace_; }
  void SetTrackedByWorkspace(bool tracked_by_workspace);

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

  // Indicates that an in progress drag should be continued after the
  // window is reparented to another container.
  bool continue_drag_after_reparent() const {
    return continue_drag_after_reparent_;
  }
  void set_continue_drag_after_reparent(bool value) {
    continue_drag_after_reparent_ = value;
  }

  // True if the window is ignored by the shelf layout manager for
  // purposes of darkening the shelf.
  bool ignored_by_shelf() const { return ignored_by_shelf_; }
  void set_ignored_by_shelf(bool ignored_by_shelf) {
    ignored_by_shelf_ = ignored_by_shelf;
  }

 private:
  // The owner of this window settings.
  aura::Window* window_;

  bool tracked_by_workspace_;
  bool window_position_managed_;
  bool bounds_changed_by_user_;
  bool panel_attached_;
  bool continue_drag_after_reparent_;
  bool ignored_by_shelf_;

  bool always_restores_to_restore_bounds_;

  // A property to remember the window position which was set before the
  // auto window position manager changed the window bounds, so that it can get
  // restored when only this one window gets shown.
  scoped_ptr<gfx::Rect> pre_auto_manage_window_bounds_;

  ObserverList<Observer> observer_list_;

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
