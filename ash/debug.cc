// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/debug.h"

#include "ash/shell.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/compositor.h"

namespace ash {
namespace debug {

void ToggleShowDebugBorders() {
  Shell::RootWindowList root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  scoped_ptr<bool> value;
  for (Shell::RootWindowList::iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    ui::Compositor* compositor = (*it)->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value.reset(new bool(!state.show_debug_borders));
    state.show_debug_borders = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

void ToggleShowFpsCounter() {
  Shell::RootWindowList root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  scoped_ptr<bool> value;
  for (Shell::RootWindowList::iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    ui::Compositor* compositor = (*it)->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value.reset(new bool(!state.show_fps_counter));
    state.show_fps_counter = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

void ToggleShowPaintRects() {
  Shell::RootWindowList root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  scoped_ptr<bool> value;
  for (Shell::RootWindowList::iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    ui::Compositor* compositor = (*it)->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value.reset(new bool(!state.show_paint_rects));
    state.show_paint_rects = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

}  // debug
}  // ash
