// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MENU_CONTAINER_LAYOUT_MANAGER_H_
#define ASH_WM_MENU_CONTAINER_LAYOUT_MANAGER_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"

namespace ash {
namespace internal {

class MenuContainerLayoutManager : public aura::LayoutManager {
 public:
  MenuContainerLayoutManager();
  virtual ~MenuContainerLayoutManager();

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuContainerLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_MENU_CONTAINER_LAYOUT_MANAGER_H_
