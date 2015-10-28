// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WM_WINDOW_LAYOUT_H_
#define COMPONENTS_MUS_EXAMPLE_WM_WINDOW_LAYOUT_H_

#include "base/macros.h"
#include "components/mus/example/wm/layout_manager.h"

// Responsible for layout of top level windows.
class WindowLayout : public LayoutManager {
 public:
  explicit WindowLayout(mus::Window* owner);
  ~WindowLayout() override;

 private:
  // Overridden from LayoutManager:
  void LayoutWindow(mus::Window* window) override;

  void FitToContainer(mus::Window* window);
  void CenterWindow(mus::Window* window, const gfx::Size& preferred_size);

  DISALLOW_COPY_AND_ASSIGN(WindowLayout);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_WINDOW_LAYOUT_H_
