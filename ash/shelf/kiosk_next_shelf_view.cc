// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/kiosk_next_shelf_view.h"

#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/overflow_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/logging.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

bool IsTabletModeEnabled() {
  return Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

}  // namespace

KioskNextShelfView::KioskNextShelfView(ShelfModel* model,
                                       Shelf* shelf,
                                       ShelfWidget* shelf_widget)
    : ShelfView(model, shelf, shelf_widget) {
  // Kiosk next shelf has 2 buttons and it is expected to be used in tablet mode
  // with bottom shelf alignment. It can be adapted to different requirements,
  // but they should be explicitly verified first.
  DCHECK(IsTabletModeEnabled());
  DCHECK(shelf->IsHorizontalAlignment());
  DCHECK_EQ(2, model->item_count());
}

KioskNextShelfView::~KioskNextShelfView() = default;

void KioskNextShelfView::Init() {
  ShelfView::Init();

  // Needs to be called after base class call that creates below objects.
  // TODO(agawronska): Separator and overflow button are not needed in Kiosk
  // Next shelf. They should be moved to DefaultShelfView subclass and the below
  // code should be removed.
  DCHECK(separator());
  DCHECK(overflow_button());
  separator()->SetVisible(false);
  overflow_button()->SetVisible(false);

  set_first_visible_index(0);
  set_last_visible_index(model()->item_count() - 1);
}

void KioskNextShelfView::CalculateIdealBounds() const {
  DCHECK(shelf()->IsHorizontalAlignment());
  DCHECK_EQ(2, model()->item_count());

  const int total_shelf_width =
      shelf_widget()->GetWindowBoundsInScreen().width();
  int x = total_shelf_width / 2 - kShelfControlSize -
          ShelfConstants::button_spacing() / 2;
  for (int i = 0; i < view_model()->view_size(); ++i) {
    view_model()->set_ideal_bounds(
        i, gfx::Rect(x, 0, kShelfControlSize, kShelfButtonSize));
    x += (kShelfControlSize + ShelfConstants::button_spacing());
  }
}

void KioskNextShelfView::LayoutAppListAndBackButtonHighlight() {
  const int total_shelf_width =
      shelf_widget()->GetWindowBoundsInScreen().width();
  const int x = total_shelf_width / 2 - kShelfControlSize -
                ShelfConstants::button_spacing() / 2;
  const int y = (ShelfConstants::shelf_size() - kShelfControlSize) / 2;
  const int back_and_app_list_background_size =
      2 * kShelfControlSize + ShelfConstants::button_spacing();
  back_and_app_list_background()->SetBounds(
      x, y, back_and_app_list_background_size, kShelfControlSize);
}

}  // namespace ash
