// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SHELF_LAYOUT_MANAGER_H_
#define ASH_WM_SHELF_LAYOUT_MANAGER_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/launcher/launcher.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindow;
}

namespace views {
class Widget;
}

namespace ash {
namespace internal {

class ShelfLayoutManagerTest;
class WorkspaceManager;

// ShelfLayoutManager is the layout manager responsible for the launcher and
// status widgets. The launcher is given the total available width and told the
// width of the status area. This allows the launcher to draw the background and
// layout to the status area.
// To respond to bounds changes in the status area StatusAreaLayoutManager works
// closely with ShelfLayoutManager.
class ASH_EXPORT ShelfLayoutManager : public aura::LayoutManager,
                                      public aura::WindowObserver {
 public:
  enum VisibilityState {
    // Completely visible.
    VISIBLE,

    // A couple of pixels are reserved at the bottom for the shelf.
    AUTO_HIDE,

    // Nothing is shown. Used for fullscreen windows.
    HIDDEN,
  };

  enum AutoHideState {
    AUTO_HIDE_SHOWN,
    AUTO_HIDE_HIDDEN,
  };

  // We reserve a small area at the bottom of the workspace area to ensure that
  // the bottom-of-window resize handle can be hit.
  // TODO(jamescook): Some day we may want the workspace area to be an even
  // multiple of the size of the grid (currently 8 pixels), which will require
  // removing this and finding a way for hover and click events to pass through
  // the invisible parts of the launcher.
  static const int kWorkspaceAreaBottomInset;

  // Size of the shelf when auto-hidden.
  static const int kAutoHideSize;

  explicit ShelfLayoutManager(views::Widget* status);
  virtual ~ShelfLayoutManager();

  // Sets the ShelfAutoHideBehavior. See enum description for details.
  void SetAutoHideBehavior(ShelfAutoHideBehavior behavior);
  ShelfAutoHideBehavior auto_hide_behavior() const {
    return auto_hide_behavior_;
  }

  // Sets the alignment.
  void SetAlignment(ShelfAlignment alignment);
  ShelfAlignment alignment() const { return alignment_; }

  void set_workspace_manager(WorkspaceManager* manager) {
    workspace_manager_ = manager;
  }

  views::Widget* launcher_widget() {
    return launcher_ ? launcher_->widget() : NULL;
  }
  const views::Widget* launcher_widget() const {
    return launcher_ ? launcher_->widget() : NULL;
  }
  views::Widget* status() { return status_; }

  bool in_layout() const { return in_layout_; }

  // Returns whether the shelf and its contents (launcher, status) are visible
  // on the screen.
  bool IsVisible() const;

  // Returns the bounds the specified window should be when maximized.
  gfx::Rect GetMaximizedWindowBounds(aura::Window* window);
  gfx::Rect GetUnmaximizedWorkAreaBounds(aura::Window* window);

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

  VisibilityState visibility_state() const { return state_.visibility_state; }
  AutoHideState auto_hide_state() const { return state_.auto_hide_state; }

  // Sets whether any windows overlap the shelf. If a window overlaps the shelf
  // the shelf renders slightly differently.
  void SetWindowOverlapsShelf(bool value);

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // Overriden from aura::WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;

 private:
  class AutoHideEventFilter;
  friend class ShelfLayoutManagerTest;

  struct TargetBounds {
    TargetBounds() : opacity(0.0f) {}

    float opacity;
    gfx::Rect launcher_bounds;
    gfx::Rect status_bounds;
    gfx::Insets work_area_insets;
  };

  struct State {
    State() : visibility_state(VISIBLE),
              auto_hide_state(AUTO_HIDE_HIDDEN),
              is_screen_locked(false) {}

    // Returns true if the two states are considered equal. As
    // |auto_hide_state| only matters if |visibility_state| is |AUTO_HIDE|,
    // Equals() ignores the |auto_hide_state| as appropriate.
    bool Equals(const State& other) const {
      return other.visibility_state == visibility_state &&
          (visibility_state != AUTO_HIDE ||
           other.auto_hide_state == auto_hide_state) &&
          other.is_screen_locked == is_screen_locked;
    }

    VisibilityState visibility_state;
    AutoHideState auto_hide_state;
    bool is_screen_locked;
  };

  // Sets the visibility of the shelf to |state|.
  void SetState(VisibilityState visibility_state);

  // Stops any animations.
  void StopAnimating();

  // Returns the width (if aligned to the side) or height (if aligned to the
  // bottom).
  void GetShelfSize(int* width, int* height);

  // Insets |bounds| by |inset| on the edge the shelf is aligned to.
  void AdjustBoundsBasedOnAlignment(int inset, gfx::Rect* bounds) const;

  // Calculates the target bounds assuming visibility of |visible|.
  void CalculateTargetBounds(const State& state, TargetBounds* target_bounds);

  // Updates the background of the shelf.
  void UpdateShelfBackground(BackgroundAnimator::ChangeType type);

  // Returns whether the launcher should draw a background.
  bool GetLauncherPaintsBackground() const;

  // Updates the auto hide state immediately.
  void UpdateAutoHideStateNow();

  // Returns the AutoHideState. This value is determined from the launcher and
  // tray.
  AutoHideState CalculateAutoHideState(VisibilityState visibility_state) const;

  // Updates the hit test bounds override for launcher and status area.
  void UpdateHitTestBounds();

  // Returns true if |window| is a descendant of the shelf.
  bool IsShelfWindow(aura::Window* window);

  int GetWorkAreaSize(const State& state, int size) const;

  int axis_position(int x, int y) const{
    return alignment_ == SHELF_ALIGNMENT_BOTTOM ? y : x;
  }

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
  views::Widget* status_;

  WorkspaceManager* workspace_manager_;

  // Do any windows overlap the shelf? This is maintained by WorkspaceManager.
  bool window_overlaps_shelf_;

  base::OneShotTimer<ShelfLayoutManager> auto_hide_timer_;

  // EventFilter used to detect when user moves the mouse over the launcher to
  // trigger showing the launcher.
  scoped_ptr<AutoHideEventFilter> event_filter_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SHELF_LAYOUT_MANAGER_H_
