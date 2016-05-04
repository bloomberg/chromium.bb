// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_SNAP_TO_PIXEL_LAYOUT_MANAGER_H_
#define ASH_WM_COMMON_WM_SNAP_TO_PIXEL_LAYOUT_MANAGER_H_

#include "ash/wm/common/ash_wm_common_export.h"
#include "ash/wm/common/wm_layout_manager.h"
#include "base/macros.h"

namespace ash {
namespace wm {

// A layout manager that places children's layer at the physical pixel
// boundaries.
class ASH_WM_COMMON_EXPORT WmSnapToPixelLayoutManager : public WmLayoutManager {
 public:
  WmSnapToPixelLayoutManager();
  ~WmSnapToPixelLayoutManager() override;

 protected:
  // Overridden from aura::LayoutManager:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(WmWindow* child) override;
  void OnWillRemoveWindowFromLayout(WmWindow* child) override;
  void OnWindowRemovedFromLayout(WmWindow* child) override;
  void OnChildWindowVisibilityChanged(WmWindow* child, bool visibile) override;
  void SetChildBounds(WmWindow* child,
                      const gfx::Rect& requested_bounds) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WmSnapToPixelLayoutManager);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_SNAP_TO_PIXEL_LAYOUT_MANAGER_H_
