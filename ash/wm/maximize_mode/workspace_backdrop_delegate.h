// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_WORKSPACE_BACKDROP_DELEGATE_H_
#define ASH_WM_MAXIMIZE_MODE_WORKSPACE_BACKDROP_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/workspace/workspace_layout_manager_backdrop_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {

// A background which gets created for a container |window| and which gets
// stacked behind the topmost window (within that container) covering the
// entire container.
class ASH_EXPORT WorkspaceBackdropDelegate
    : public WorkspaceLayoutManagerBackdropDelegate {
 public:
  explicit WorkspaceBackdropDelegate(aura::Window* container);
  ~WorkspaceBackdropDelegate() override;

  // WorkspaceLayoutManagerBackdropDelegate overrides:
  void OnWindowAddedToLayout(wm::WmWindow* child) override;
  void OnWindowRemovedFromLayout(wm::WmWindow* child) override;
  void OnChildWindowVisibilityChanged(wm::WmWindow* child,
                                      bool visible) override;
  void OnWindowStackingChanged(wm::WmWindow* window) override;
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   wm::WindowStateType old_type) override;
  void OnDisplayWorkAreaInsetsChanged() override;

 private:
  class WindowObserverImpl;

  // Restack the backdrop relatively to the other windows in the container.
  void RestackBackdrop();

  // Returns the current visible top level window in the container.
  aura::Window* GetCurrentTopWindow();

  // Position & size the background over the container window.
  void AdjustToContainerBounds();

  // Show the overlay.
  void Show();

  std::unique_ptr<WindowObserverImpl> container_observer_;

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
