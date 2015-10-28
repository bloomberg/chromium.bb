// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WM_SHELF_LAYOUT_H_
#define COMPONENTS_MUS_EXAMPLE_WM_SHELF_LAYOUT_H_

#include "base/macros.h"
#include "components/mus/example/wm/layout_manager.h"

// Lays out the shelf within shelf containers.
class ShelfLayout : public LayoutManager {
 public:
  explicit ShelfLayout(mus::Window* owner);
  ~ShelfLayout() override;

 private:
  // Overridden from LayoutManager:
  void WindowAdded(mus::Window* window) override;
  void LayoutWindow(mus::Window* window) override;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayout);
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_SHELF_LAYOUT_H_
