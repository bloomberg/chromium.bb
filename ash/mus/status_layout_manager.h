// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_STATUS_LAYOUT_MANAGER_H_
#define ASH_MUS_STATUS_LAYOUT_MANAGER_H_

#include "ash/mus/layout_manager.h"
#include "base/macros.h"
#include "mash/shelf/public/interfaces/shelf_constants.mojom.h"

namespace ash {
namespace mus {

// Lays out the status container, which contains the status area widget and
// notifications.
class StatusLayoutManager : public LayoutManager {
 public:
  explicit StatusLayoutManager(::ui::Window* owner);
  ~StatusLayoutManager() override;

  void SetAlignment(mash::shelf::mojom::Alignment alignment);
  void SetAutoHideBehavior(mash::shelf::mojom::AutoHideBehavior auto_hide);

 private:
  // Overridden from LayoutManager:
  void LayoutWindow(::ui::Window* window) override;

  mash::shelf::mojom::Alignment alignment_;
  mash::shelf::mojom::AutoHideBehavior auto_hide_behavior_;

  DISALLOW_COPY_AND_ASSIGN(StatusLayoutManager);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_STATUS_LAYOUT_MANAGER_H_
