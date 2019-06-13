// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/default_shelf_view.h"

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/views/view.h"

namespace ash {

namespace {

bool IsTabletModeEnabled() {
  // This check is needed, because tablet mode controller is destroyed before
  // shelf widget. See https://crbug.com/967149 for more details.
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()
             ->tablet_mode_controller()
             ->IsTabletModeWindowManagerEnabled();
}

}  // namespace

DefaultShelfView::DefaultShelfView(ShelfModel* model,
                                   Shelf* shelf,
                                   ShelfWidget* shelf_widget)
    : ShelfView(model, shelf, shelf_widget) {}

DefaultShelfView::~DefaultShelfView() = default;

void DefaultShelfView::Init() {
  // Add the background view behind the app list and back buttons first, so
  // that other views will appear above it.
  back_and_app_list_background_ = new views::View();
  back_and_app_list_background_->set_can_process_events_within_subtree(false);
  back_and_app_list_background_->SetBackground(
      CreateBackgroundFromPainter(views::Painter::CreateSolidRoundRectPainter(
          kShelfControlPermanentHighlightBackground,
          ShelfConstants::control_border_radius())));
  ConfigureChildView(back_and_app_list_background_);
  AddChildView(back_and_app_list_background_);

  // Calling Init() after adding background view, so control buttons are added
  // after background.
  ShelfView::Init();
}

void DefaultShelfView::LayoutAppListAndBackButtonHighlight() {
  // Don't show anything if this is the overflow menu.
  if (is_overflow_mode()) {
    back_and_app_list_background_->SetVisible(false);
    return;
  }
  const int button_spacing = ShelfConstants::button_spacing();
  // "Secondary" as in "orthogonal to the shelf primary axis".
  const int control_secondary_padding =
      (ShelfConstants::shelf_size() - kShelfControlSize) / 2;
  const int back_and_app_list_background_size =
      kShelfControlSize +
      (IsTabletModeEnabled() ? kShelfControlSize + button_spacing : 0);
  back_and_app_list_background_->SetBounds(
      shelf()->PrimaryAxisValue(button_spacing, control_secondary_padding),
      shelf()->PrimaryAxisValue(control_secondary_padding, button_spacing),
      shelf()->PrimaryAxisValue(back_and_app_list_background_size,
                                kShelfControlSize),
      shelf()->PrimaryAxisValue(kShelfControlSize,
                                back_and_app_list_background_size));
}

}  // namespace ash
