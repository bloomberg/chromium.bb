// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_SHELF_LAYOUT_H_
#define MASH_WM_SHELF_LAYOUT_H_

#include "base/macros.h"
#include "mash/wm/layout_manager.h"
#include "mash/wm/public/interfaces/shelf_layout.mojom.h"

namespace mash {
namespace wm {

// Lays out the shelf within shelf containers.
class ShelfLayout : public LayoutManager, public mojom::ShelfLayout {
 public:
  explicit ShelfLayout(mus::Window* owner);
  ~ShelfLayout() override;

 private:
  // Overridden from LayoutManager:
  void LayoutWindow(mus::Window* window) override;

  // Overridden from mojom::ShelfLayout:
  void SetAlignment(mash::shelf::mojom::Alignment alignment) override;
  void SetAutoHideBehavior(
      mash::shelf::mojom::AutoHideBehavior auto_hide) override;

  mash::shelf::mojom::Alignment alignment_;
  mash::shelf::mojom::AutoHideBehavior auto_hide_behavior_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayout);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_SHELF_LAYOUT_H_
