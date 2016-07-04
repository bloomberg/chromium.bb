// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SHELF_LAYOUT_MANAGER_H_
#define ASH_MUS_SHELF_LAYOUT_MANAGER_H_

#include "ash/mus/layout_manager.h"
#include "base/macros.h"
#include "mash/shelf/public/interfaces/shelf_constants.mojom.h"

namespace ash {
namespace mus {

class ShelfLayoutManagerDelegate;

// Lays out the shelf within shelf containers.
class ShelfLayoutManager : public LayoutManager {
 public:
  ShelfLayoutManager(::ui::Window* owner, ShelfLayoutManagerDelegate* delegate);
  ~ShelfLayoutManager() override;

  // Returns the shelf, which may be null.
  ::ui::Window* GetShelfWindow();

  void SetAlignment(mash::shelf::mojom::Alignment alignment);
  void SetAutoHideBehavior(mash::shelf::mojom::AutoHideBehavior auto_hide);

  mash::shelf::mojom::Alignment alignment() const { return alignment_; }
  mash::shelf::mojom::AutoHideBehavior auto_hide_behavior() const {
    return auto_hide_behavior_;
  }

 private:
  // Overridden from LayoutManager:
  void LayoutWindow(::ui::Window* window) override;
  void WindowAdded(::ui::Window* window) override;

  ShelfLayoutManagerDelegate* delegate_;
  mash::shelf::mojom::Alignment alignment_;
  mash::shelf::mojom::AutoHideBehavior auto_hide_behavior_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManager);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_SHELF_LAYOUT_MANAGER_H_
