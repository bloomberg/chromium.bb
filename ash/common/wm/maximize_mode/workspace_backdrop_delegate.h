// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_MAXIMIZE_MODE_WORKSPACE_BACKDROP_DELEGATE_H_
#define ASH_COMMON_WM_MAXIMIZE_MODE_WORKSPACE_BACKDROP_DELEGATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/wm/workspace/workspace_layout_manager_backdrop_delegate.h"
#include "base/macros.h"

namespace views {
class Widget;
}

namespace ash {

class WmWindow;

// A background which gets created for a container |window| and which gets
// stacked behind the topmost window (within that container) covering the
// entire container.
class ASH_EXPORT WorkspaceBackdropDelegate
    : public WorkspaceLayoutManagerBackdropDelegate {
 public:
  explicit WorkspaceBackdropDelegate(WmWindow* container);
  ~WorkspaceBackdropDelegate() override;

  // WorkspaceLayoutManagerBackdropDelegate overrides:
  void OnWindowAddedToLayout(WmWindow* child) override;
  void OnWindowRemovedFromLayout(WmWindow* child) override;
  void OnChildWindowVisibilityChanged(WmWindow* child, bool visible) override;
  void OnWindowStackingChanged(WmWindow* window) override;
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   wm::WindowStateType old_type) override;

 private:
  // Restack the backdrop relatively to the other windows in the container.
  void RestackBackdrop();

  // Returns the current visible top level window in the container.
  WmWindow* GetCurrentTopWindow();

  // Show the overlay.
  void Show();

  // The background which covers the rest of the screen.
  views::Widget* background_ = nullptr;
  // WmWindow for |background_|.
  WmWindow* background_window_ = nullptr;

  // The window which is being "maximized".
  WmWindow* container_;

  // If true, the |RestackOrHideWindow| might recurse.
  bool in_restacking_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceBackdropDelegate);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_MAXIMIZE_MODE_WORKSPACE_BACKDROP_DELEGATE_H_
