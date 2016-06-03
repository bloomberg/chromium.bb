// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SHELF_LAYOUT_IMPL_H_
#define ASH_MUS_SHELF_LAYOUT_IMPL_H_

#include "ash/public/interfaces/shelf_layout.mojom.h"
#include "base/macros.h"

namespace ash {
namespace mus {

class RootWindowController;

// Implements the ShelfLayout mojo interface to listen for layout changes from
// the system UI application.
class ShelfLayoutImpl : public mojom::ShelfLayout {
 public:
  ShelfLayoutImpl();
  ~ShelfLayoutImpl() override;

  void Initialize(RootWindowController* root_controller);

 private:
  // Overridden from mojom::ShelfLayout:
  void SetAlignment(mash::shelf::mojom::Alignment alignment) override;
  void SetAutoHideBehavior(
      mash::shelf::mojom::AutoHideBehavior auto_hide) override;

  RootWindowController* root_controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutImpl);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_SHELF_LAYOUT_IMPL_H_
