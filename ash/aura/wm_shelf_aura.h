// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_WM_SHELF_AURA_H_
#define ASH_AURA_WM_SHELF_AURA_H_

#include "ash/ash_export.h"
#include "ash/common/shelf/wm_shelf.h"
#include "base/macros.h"

namespace ash {

class ShelfBezelEventHandler;

// Aura implementation of WmShelf.
class ASH_EXPORT WmShelfAura : public WmShelf {
 public:
  WmShelfAura();
  ~WmShelfAura() override;

  // WmShelf:
  WmDimmerView* CreateDimmerView(bool disable_animations_for_test) override;
  void SetShelfLayoutManager(ShelfLayoutManager* manager) override;
  void WillDeleteShelfLayoutManager() override;
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;

 private:
  class AutoHideEventHandler;

  // Forwards mouse and gesture events to ShelfLayoutManager for auto-hide.
  // TODO(mash): Facilitate simliar functionality in mash: crbug.com/631216
  std::unique_ptr<AutoHideEventHandler> auto_hide_event_handler_;

  // Forwards touch gestures on a bezel sensor to the shelf.
  // TODO(mash): Facilitate simliar functionality in mash: crbug.com/636647
  std::unique_ptr<ShelfBezelEventHandler> bezel_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(WmShelfAura);
};

}  // namespace ash

#endif  // ASH_AURA_WM_SHELF_AURA_H_
