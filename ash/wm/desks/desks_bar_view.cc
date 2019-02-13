// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_bar_view.h"

#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/new_desk_button.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

DesksBarView::DesksBarView() : new_desk_button_(new NewDeskButton(this)) {
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetColor(SkColorSetARGB(60, 0, 0, 0));

  AddChildView(new_desk_button_);
  new_desk_button_->SetEnabled(DesksController::Get()->CanCreateDesks());
}

// static
int DesksBarView::GetBarHeight() {
  // TODO(afakhry): Bar expands when we add the second desk.
  return 48;
}

const char* DesksBarView::GetClassName() const {
  return "DesksBarView";
}

void DesksBarView::Layout() {
  constexpr int kButtonRightMargin = 36;
  constexpr int kIconAndTextHorizontalPadding = 16;
  constexpr int kIconAndTextVerticalPadding = 8;

  gfx::Size new_desk_button_size = new_desk_button_->GetPreferredSize();
  new_desk_button_size.Enlarge(2 * kIconAndTextHorizontalPadding,
                               2 * kIconAndTextVerticalPadding);

  const gfx::Rect button_bounds{
      bounds().right() - new_desk_button_size.width() - kButtonRightMargin,
      (bounds().height() - new_desk_button_size.height()) / 2,
      new_desk_button_size.width(), new_desk_button_size.height()};
  new_desk_button_->SetBoundsRect(button_bounds);

  // TODO(afakhry): Layout thumbnails.
}

void DesksBarView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  auto* controller = DesksController::Get();
  if (sender == new_desk_button_ && controller->CanCreateDesks()) {
    controller->NewDesk();
    return;
  }

  // TODO(afakhry): Handle thumbnail presses.
}

}  // namespace ash
