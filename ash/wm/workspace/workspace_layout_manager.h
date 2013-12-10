// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_

#include <set>

#include "ash/shell_observer.h"
#include "ash/wm/base_layout_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ui_base_types.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/rect.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ui {
class Layer;
}

namespace ash {

namespace internal {

class ShelfLayoutManager;

// LayoutManager used on the window created for a workspace.
class ASH_EXPORT WorkspaceLayoutManager : public BaseLayoutManager {
 public:
  explicit WorkspaceLayoutManager(aura::Window* window);
  virtual ~WorkspaceLayoutManager();

  void SetShelf(internal::ShelfLayoutManager* shelf);

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {}
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // ash::ShellObserver overrides:
  virtual void OnDisplayWorkAreaInsetsChanged() OVERRIDE;

  // Overriden from WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowStackingChanged(aura::Window* window) OVERRIDE;

  // WindowStateObserver overrides:
  virtual void OnWindowShowTypeChanged(wm::WindowState* window_state,
                                       wm::WindowShowType old_type) OVERRIDE;

 private:
  // Overridden from BaseLayoutManager:
  virtual void ShowStateChanged(wm::WindowState* window_state,
                                ui::WindowShowState last_show_state) OVERRIDE;
  virtual void AdjustAllWindowsBoundsForWorkAreaChange(
      AdjustWindowReason reason) OVERRIDE;
  virtual void AdjustWindowBoundsForWorkAreaChange(
      wm::WindowState* window_state,
      AdjustWindowReason reason) OVERRIDE;

  void AdjustWindowBoundsWhenAdded(wm::WindowState* window_state);

  // Updates the visibility state of the shelf.
  void UpdateShelfVisibility();

  // Updates the fullscreen state of the workspace and notifies Shell if it
  // has changed.
  void UpdateFullscreenState();

  // Updates the bounds of the window for a show state change from
  // |last_show_state|.
  void UpdateBoundsFromShowState(wm::WindowState* window_state,
                                 ui::WindowShowState last_show_state);

  // If |window_state| is maximized or fullscreen the bounds of the
  // window are set and true is returned. Does nothing otherwise.
  bool SetMaximizedOrFullscreenBounds(wm::WindowState* window_state);

  // Adjusts the |bounds| so that they are flush with the edge of the
  // workspace if the window represented by |window_state| is side snapped.
  void AdjustSnappedBounds(wm::WindowState* window_state, gfx::Rect* bounds);

  // Animates the window bounds to |bounds|.
  void SetChildBoundsAnimated(aura::Window* child, const gfx::Rect& bounds);

  internal::ShelfLayoutManager* shelf_;
  aura::Window* window_;

  // The work area. Cached to avoid unnecessarily moving windows during a
  // workspace switch.
  gfx::Rect work_area_in_parent_;

  // True if this workspace is currently in fullscreen mode.
  bool is_fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_LAYOUT_MANAGER_H_
