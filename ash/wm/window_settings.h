// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_SETTINGS_H_
#define ASH_WM_WINDOW_SETTINGS_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/observer_list.h"

namespace aura {
class Window;
}

namespace ash {
namespace wm {

// Per managed window information should be stored here
// instead of using plain aura window property.
class ASH_EXPORT WindowSettings {
 public:
  class Observer {
   public:
    // Called when the tracked_by_workspace has changed.
    virtual void OnTrackedByWorkspaceChanged(aura::Window* window,
                                             bool old_value) {}
  };

  explicit WindowSettings(aura::Window* window);
  ~WindowSettings();

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

  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(WindowSettings);
};

// Returns the WindowSettings for |window|. Creates WindowSettings
// if it didn't exist. The settings object is owned by |window|.
ASH_EXPORT WindowSettings* GetWindowSettings(aura::Window* window);

// const version of GetWindowSettings.
ASH_EXPORT const WindowSettings*
GetWindowSettings(const aura::Window* window);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_SETTINGS_H_
