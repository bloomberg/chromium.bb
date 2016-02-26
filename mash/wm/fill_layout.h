// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_FILL_LAYOUT_H_
#define MASH_WM_FILL_LAYOUT_H_

#include "base/macros.h"
#include "mash/wm/layout_manager.h"

namespace mash {
namespace wm {

// Lays out windows to fill their containers.
class FillLayout : public LayoutManager {
 public:
  explicit FillLayout(mus::Window* owner);
  ~FillLayout() override;

 private:
  // Overridden from LayoutManager:
  void LayoutWindow(mus::Window* window) override;

  DISALLOW_COPY_AND_ASSIGN(FillLayout);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_FILL_LAYOUT_H_
