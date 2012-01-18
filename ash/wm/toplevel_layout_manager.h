// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TOPLEVEL_LAYOUT_MANAGER_H_
#define ASH_WM_TOPLEVEL_LAYOUT_MANAGER_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/wm/base_layout_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {
namespace internal {

class ShelfLayoutManager;

// ToplevelLayoutManager is the LayoutManager installed on a container that
// hosts what the shell considers to be top-level windows.
//
// ToplevelLayoutManager is used if the WorkspaceManager is not enabled and
// compact window mode is not enabled. It is intended to implement the simplest
// possible window management code. If you have a more complex window mode
// please implement a new LayoutManager for it.
class ASH_EXPORT ToplevelLayoutManager : public BaseLayoutManager {
 public:
  ToplevelLayoutManager();
  virtual ~ToplevelLayoutManager();

  void set_shelf(ShelfLayoutManager* shelf) { shelf_ = shelf; }

  // LayoutManager overrides:
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const char* name,
                                       void* old) OVERRIDE;

 private:
  // Updates the visibility of the shelf based on if there are any full screen
  // windows.
  void UpdateShelfVisibility();

  // Owned by the Shell container window LauncherContainer. May be NULL if
  // we're not using a shelf.
  ShelfLayoutManager* shelf_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelLayoutManager);
};

}  // namespace ash
}  // namespace internal

#endif  // ASH_WM_TOPLEVEL_LAYOUT_MANAGER_H_
