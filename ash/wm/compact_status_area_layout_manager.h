// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMPACT_STATUS_AREA_LAYOUT_MANAGER_H_
#define ASH_WM_COMPACT_STATUS_AREA_LAYOUT_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"

namespace views {
class Widget;
}

namespace aura_shell {
namespace internal {

// CompactStatusAreaLayoutManager places the status area in the top-right
// corner of the screen.
class CompactStatusAreaLayoutManager : public aura::LayoutManager {
 public:
  explicit CompactStatusAreaLayoutManager(views::Widget* status_widget);
  virtual ~CompactStatusAreaLayoutManager();

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

 private:
  // Place the status area widget in the corner of the screen.
  void LayoutStatusArea();

  views::Widget* status_widget_;

  DISALLOW_COPY_AND_ASSIGN(CompactStatusAreaLayoutManager);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // ASH_WM_COMPACT_STATUS_AREA_LAYOUT_MANAGER_H_
