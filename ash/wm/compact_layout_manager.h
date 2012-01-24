// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMPACT_LAYOUT_MANAGER_H_
#define ASH_WM_COMPACT_LAYOUT_MANAGER_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/wm/base_layout_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/compositor/layer_animation_observer.h"

namespace views {
class Widget;
}

namespace ash {
namespace internal {

// CompactLayoutManager is the LayoutManager used in compact window mode,
// which emulates the traditional Chrome OS window manager.  Windows are always
// maximized, fill the screen, and only one tabbed browser window is visible at
// a time.  The status area appears in the top-right corner of the screen.
class ASH_EXPORT CompactLayoutManager : public BaseLayoutManager,
                                        public ui::LayerAnimationObserver {
 public:
  CompactLayoutManager();
  virtual ~CompactLayoutManager();

  void set_status_area_widget(views::Widget* widget) {
    status_area_widget_ = widget;
  }

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
  virtual void OnWindowStackingChanged(aura::Window* window) OVERRIDE;

  // ui::LayerAnimationObserver:
  virtual void OnLayerAnimationEnded(
      const ui::LayerAnimationSequence* animation) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      const ui::LayerAnimationSequence* animation) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      const ui::LayerAnimationSequence* animation) OVERRIDE;

 private:
  // Hides the status area if we are managing it and full screen windows are
  // visible.
  void UpdateStatusAreaVisibility();

  // Start a slide in / out animation sequence for the layer.
  // The underlying layer is translated to -|offset_x| position.
  void AnimateSlideTo(int offset_x);

  // Layout all browser windows currently in the window cycle list.
  // skip |skip_this_window| if we do not want the window to be laid out.
  int LayoutWindows(aura::Window* skip_this_window);

  // Hides all but the |current_window_| in the windows list. If we
  // cannot determine the |current_window_|, we do not hide any.
  void HideWindows();

  // Returns the first window in the window cycle list.
  aura::Window* FindFirstWindow();

  // Status area with clock, network, battery, etc. icons. May be NULL if the
  // shelf is managing the status area.
  views::Widget* status_area_widget_;

  // The browser window currently in the viewport.
  aura::Window* current_window_;

  DISALLOW_COPY_AND_ASSIGN(CompactLayoutManager);
};

}  // namespace ash
}  // namespace internal

#endif  // ASH_WM_COMPACT_LAYOUT_MANAGER_H_
