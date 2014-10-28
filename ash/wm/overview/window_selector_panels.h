// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_PANELS_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_PANELS_H_

#include "ash/wm/overview/window_selector_item.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class ScopedTransformOverviewWindow;

// This class implements a window selector item containing one or more attached
// panel windows. These panels are grouped into a single overview item in
// overview mode and the callout arrows are hidden at this point.
class WindowSelectorPanels : public WindowSelectorItem {
 public:
  explicit WindowSelectorPanels(aura::Window* panels_root_window);
  ~WindowSelectorPanels() override;

  // Adds |window| to the selector item. This window should be an attached
  // panel window.
  void AddWindow(aura::Window* window);

  // WindowSelectorItem:
  aura::Window* GetRootWindow() override;
  bool HasSelectableWindow(const aura::Window* window) override;
  bool Contains(const aura::Window* target) override;
  void RestoreWindowOnExit(aura::Window* window) override;
  aura::Window* SelectionWindow() override;
  void RemoveWindow(const aura::Window* window) override;
  bool empty() const override;
  void PrepareForOverview() override;
  void SetItemBounds(aura::Window* root_window,
                     const gfx::Rect& target_bounds,
                     bool animate) override;
  void SetOpacity(float opacity) override;

 private:
  typedef ScopedVector<ScopedTransformOverviewWindow> WindowList;
  WindowList transform_windows_;

  // The root window of the panels this item contains.
  aura::Window* panels_root_window_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorPanels);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_PANELS_H_
