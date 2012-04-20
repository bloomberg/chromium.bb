// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ROOT_WINDOW_LAYOUT_MANAGER_H_
#define ASH_WM_ROOT_WINDOW_LAYOUT_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/layout_manager.h"

namespace aura {
class Window;
}
namespace gfx {
class Rect;
}
namespace ui {
class Layer;
}
namespace views {
class Widget;
}

namespace ash {
namespace internal {

// A layout manager for the root window.
// Resizes all of its immediate children to fill the bounds of the root window.
class RootWindowLayoutManager : public aura::LayoutManager {
 public:
  explicit RootWindowLayoutManager(aura::Window* owner);
  virtual ~RootWindowLayoutManager();

  views::Widget* background_widget() { return background_widget_; }
  ui::Layer* background_layer() { return background_layer_.get(); }

  // Sets the background to |widget|. Closes and destroys the old widget if it
  // exists and differs from the new widget.
  void SetBackgroundWidget(views::Widget* widget);

  // Sets a background layer, taking ownership of |layer|.  This is provided as
  // a lightweight alternative to SetBackgroundWidget(); layers can be simple
  // colored quads instead of being textured.
  void SetBackgroundLayer(ui::Layer* layer);

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

 private:
  aura::Window* owner_;

  // May be NULL if we're not painting a background.
  views::Widget* background_widget_;
  scoped_ptr<ui::Layer> background_layer_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_ROOT_WINDOW_LAYOUT_MANAGER_H_
