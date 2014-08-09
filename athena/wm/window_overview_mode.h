// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_WINDOW_OVERVIEW_MODE_H_
#define ATHENA_WM_WINDOW_OVERVIEW_MODE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/aura/window.h"

namespace athena {
class WindowListProvider;

class WindowOverviewModeDelegate {
 public:
  virtual ~WindowOverviewModeDelegate() {}

  virtual void OnSelectWindow(aura::Window* window) = 0;
};

class WindowOverviewMode {
 public:
  virtual ~WindowOverviewMode() {}

  static scoped_ptr<WindowOverviewMode> Create(
      aura::Window* container,
      const WindowListProvider* window_list_provider,
      WindowOverviewModeDelegate* delegate);
};

}  // namespace athena

#endif  // ATHENA_WM_WINDOW_OVERVIEW_MODE_H_
