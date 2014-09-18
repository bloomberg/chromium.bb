// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_WINDOW_OVERVIEW_MODE_H_
#define ATHENA_WM_WINDOW_OVERVIEW_MODE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/aura/window.h"

namespace athena {
class SplitViewController;
class WindowListProvider;

class WindowOverviewModeDelegate {
 public:
  virtual ~WindowOverviewModeDelegate() {}

  // Called to activate |window|, set its bounds and set its visibility when
  // |window| is selected in overview mode. |window| is NULL if there are no
  // windows in overview mode.
  virtual void OnSelectWindow(aura::Window* window) = 0;

  // Gets into split-view mode with |left| on the left-side of the screen, and
  // |right| on the right-side. If |left| or |right| is NULL, then the delegate
  // selects the best option in its place.
  virtual void OnSelectSplitViewWindow(aura::Window* left,
                                       aura::Window* right,
                                       aura::Window* to_activate) = 0;
};

class WindowOverviewMode {
 public:
  virtual ~WindowOverviewMode() {}

  static scoped_ptr<WindowOverviewMode> Create(
      aura::Window* container,
      WindowListProvider* window_list_provider,
      SplitViewController* split_view_controller,
      WindowOverviewModeDelegate* delegate);
};

}  // namespace athena

#endif  // ATHENA_WM_WINDOW_OVERVIEW_MODE_H_
