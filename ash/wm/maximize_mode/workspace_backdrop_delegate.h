// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_WORKSPACE_BACKDROP_DELEGATE_H_
#define ASH_WM_MAXIMIZE_MODE_WORKSPACE_BACKDROP_DELEGATE_H_

#include "ash/wm/workspace/workspace_layout_manager_delegate.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ui {
class Layer;
}

namespace views {
class Widget;
}

namespace ash {

// A background which gets created for a container |window| and which gets
// stacked behind the topmost window (within that container) covering the
// entire container.
class WorkspaceBackdropDelegate : public aura::WindowObserver,
                                  public WorkspaceLayoutManagerDelegate {
 public:
  explicit WorkspaceBackdropDelegate(aura::Window* container);
  virtual ~WorkspaceBackdropDelegate();

  // WindowObserver overrides:
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // WorkspaceLayoutManagerDelegate overrides:
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void OnWindowStackingChanged(aura::Window* window) OVERRIDE;
  virtual void OnPostWindowStateTypeChange(
      wm::WindowState* window_state,
      wm::WindowStateType old_type) OVERRIDE;
  virtual void OnDisplayWorkAreaInsetsChanged() OVERRIDE;

 private:
  // Restack the backdrop relatively to the other windows in the container.
  void RestackBackdrop();

  // Returns the current visible top level window in the container.
  aura::Window* GetCurrentTopWindow();

  // Position & size the background over the container window.
  void AdjustToContainerBounds();

  // Show the overlay.
  void Show();

  // The background which covers the rest of the screen.
  views::Widget* background_;

  // The window which is being "maximized".
  aura::Window* container_;

  // If true, the |RestackOrHideWindow| might recurse.
  bool in_restacking_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceBackdropDelegate);
};

}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_WORKSPACE_BACKDROP_DELEGATE_H_
