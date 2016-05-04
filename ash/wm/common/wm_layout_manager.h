// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_LAYOUT_MANAGER_H_
#define ASH_WM_COMMON_WM_LAYOUT_MANAGER_H_

#include "ash/wm/common/ash_wm_common_export.h"

namespace gfx {
class Rect;
}

namespace ash {
namespace wm {

class WmWindow;

// Used to position the child WmWindows of a WmWindow. WmLayoutManager is called
// at key points in a WmWindows life cycle thats allow sthe WmLayoutManager to
// position or change child WmWindows.
class ASH_WM_COMMON_EXPORT WmLayoutManager {
 public:
  virtual ~WmLayoutManager() {}

  // Invoked when the window the WmLayoutManager is associated with is resized.
  virtual void OnWindowResized() = 0;

  // Invoked when a window is added to the window the WmLayoutManager is
  // associated with.
  virtual void OnWindowAddedToLayout(WmWindow* child) = 0;

  // Invoked prior to removing |child|.
  virtual void OnWillRemoveWindowFromLayout(WmWindow* child) = 0;

  // Invoked after removing |child|.
  virtual void OnWindowRemovedFromLayout(WmWindow* child) = 0;

  // Invoked when SetVisible() is invoked on |child|. |visible| is the value
  // supplied to SetVisible(). If |visible| is true, child->IsVisible() may
  // still return false. See description in aura::Window::IsVisible() for
  // details.
  virtual void OnChildWindowVisibilityChanged(WmWindow* child,
                                              bool visibile) = 0;

  // Invoked when WmWindow::SetBounds() is called on |child|. This allows the
  // WmLayoutManager to modify the bounds as appropriate before processing the
  // request, including ignoring the request. Implementation must call
  // SetChildBoundsDirect() so that an infinite loop does not result.
  virtual void SetChildBounds(WmWindow* child,
                              const gfx::Rect& requested_bounds) = 0;
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_LAYOUT_MANAGER_H_
