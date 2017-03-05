// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_ROOT_WINDOW_LAYOUT_MANAGER_H_
#define ASH_COMMON_WM_ROOT_WINDOW_LAYOUT_MANAGER_H_

#include "ash/common/wm_layout_manager.h"
#include "base/macros.h"

namespace ash {
namespace wm {

// A layout manager for the root window.
// Resizes all of its immediate children and their descendants to fill the
// bounds of the associated window.
class RootWindowLayoutManager : public WmLayoutManager {
 public:
  explicit RootWindowLayoutManager(WmWindow* owner);
  ~RootWindowLayoutManager() override;

  // Overridden from aura::LayoutManager:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(WmWindow* child) override;
  void OnWillRemoveWindowFromLayout(WmWindow* child) override;
  void OnWindowRemovedFromLayout(WmWindow* child) override;
  void OnChildWindowVisibilityChanged(WmWindow* child, bool visible) override;
  void SetChildBounds(WmWindow* child,
                      const gfx::Rect& requested_bounds) override;

 private:
  WmWindow* owner_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowLayoutManager);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_COMMON_WM_ROOT_WINDOW_LAYOUT_MANAGER_H_
