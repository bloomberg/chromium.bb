// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_KIOSK_NEXT_SHELF_VIEW_H_
#define ASH_SHELF_KIOSK_NEXT_SHELF_VIEW_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_view.h"
#include "base/macros.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class Shelf;
struct ShelfItem;
class ShelfModel;
class ShelfWidget;

// Shelf view used in Kiosk Next mode.
// This shelf view is much simpler than the default shelf view. It is always
// bottom aligned. It shows centered control buttons (back and home). It does
// not display apps' buttons. Because of this alignment overflow mode is not
// needed. This shelf view is using ShelfModel, but it could be removed in the
// future.
class ASH_EXPORT KioskNextShelfView : public ShelfView {
 public:
  KioskNextShelfView(ShelfModel* model,
                     Shelf* shelf,
                     ShelfWidget* shelf_widget);
  ~KioskNextShelfView() override;

  // All ShelfView overrides are public to keep them together.
  // ShelfView:
  void Init() override;
  void CalculateIdealBounds() override;
  views::View* CreateViewForItem(const ShelfItem& item) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskNextShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_KIOSK_NEXT_SHELF_VIEW_H_
