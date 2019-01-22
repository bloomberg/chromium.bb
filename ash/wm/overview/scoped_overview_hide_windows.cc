// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_hide_windows.h"

#include "base/logging.h"
#include "ui/aura/window.h"

namespace ash {

ScopedOverviewHideWindows::ScopedOverviewHideWindows(
    const std::vector<aura::Window*>& windows) {
  for (auto* window : windows) {
    window->AddObserver(this);
    window_visibility_.emplace(window, window->IsVisible());
    window->Hide();
  }
}

ScopedOverviewHideWindows::~ScopedOverviewHideWindows() {
  for (auto iter = window_visibility_.begin(); iter != window_visibility_.end();
       iter++) {
    iter->first->RemoveObserver(this);
    if (iter->second)
      iter->first->Show();
  }
}

void ScopedOverviewHideWindows::OnWindowDestroying(aura::Window* window) {
  window_visibility_.erase(window);
}

void ScopedOverviewHideWindows::OnWindowVisibilityChanged(aura::Window* window,
                                                          bool visible) {
  // It's expected that windows hidden in overview should not make them visible
  // without exiting overview.
  if (visible)
    NOTREACHED();
}

}  // namespace ash