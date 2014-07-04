// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_WINDOW_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_WINDOW_H_

#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/window_selector_item.h"
#include "base/compiler_specific.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {

// This implements a window overview item with a single window which can be
// selected.
class WindowSelectorWindow : public WindowSelectorItem {
 public:
  explicit WindowSelectorWindow(aura::Window* window);
  virtual ~WindowSelectorWindow();

  // WindowSelectorItem:
  virtual aura::Window* GetRootWindow() OVERRIDE;
  virtual bool HasSelectableWindow(const aura::Window* window) OVERRIDE;
  virtual bool Contains(const aura::Window* target) OVERRIDE;
  virtual void RestoreWindowOnExit(aura::Window* window) OVERRIDE;
  virtual aura::Window* SelectionWindow() OVERRIDE;
  virtual void RemoveWindow(const aura::Window* window) OVERRIDE;
  virtual bool empty() const OVERRIDE;
  virtual void PrepareForOverview() OVERRIDE;
  virtual void SetItemBounds(aura::Window* root_window,
                             const gfx::Rect& target_bounds,
                             bool animate) OVERRIDE;
  virtual void SetOpacity(float opacity) OVERRIDE;

 private:
  // The window with a scoped transform represented by this selector item.
  ScopedTransformOverviewWindow transform_window_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorWindow);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_WINDOW_H_
