// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_views.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ui/gfx/font.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace internal {

void SetupLabelForTray(views::Label* label) {
  // Making label_font static to avoid the time penalty of DeriveFont for
  // all but the first call.
  static const gfx::Font label_font(gfx::Font().DeriveFont(1, gfx::Font::BOLD));
  label->SetFont(label_font);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(SK_ColorWHITE);
  label->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
  label->SetShadowColors(SkColorSetARGB(64, 0, 0, 0),
                         SkColorSetARGB(64, 0, 0, 0));
  label->SetShadowOffset(0, 1);
}

void SetTrayImageItemBorder(views::View* tray_view,
                            ShelfAlignment alignment) {
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_TOP) {
    tray_view->set_border(views::Border::CreateEmptyBorder(
        0, kTrayImageItemHorizontalPaddingBottomAlignment,
        0, kTrayImageItemHorizontalPaddingBottomAlignment));
  } else {
    tray_view->set_border(views::Border::CreateEmptyBorder(
        kTrayImageItemVerticalPaddingVerticalAlignment,
        kTrayImageItemHorizontalPaddingVerticalAlignment,
        kTrayImageItemVerticalPaddingVerticalAlignment,
        kTrayImageItemHorizontalPaddingVerticalAlignment));
  }
}

void SetTrayLabelItemBorder(TrayItemView* tray_view,
                            ShelfAlignment alignment) {
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_TOP) {
    tray_view->set_border(views::Border::CreateEmptyBorder(
        0, kTrayLabelItemHorizontalPaddingBottomAlignment,
        0, kTrayLabelItemHorizontalPaddingBottomAlignment));
  } else {
    // Center the label for vertical launcher alignment.
    int horizontal_padding = (tray_view->GetPreferredSize().width() -
        tray_view->label()->GetPreferredSize().width()) / 2;
    tray_view->set_border(views::Border::CreateEmptyBorder(
        kTrayLabelItemVerticalPaddingVeriticalAlignment,
        horizontal_padding,
        kTrayLabelItemVerticalPaddingVeriticalAlignment,
        horizontal_padding));
  }
}

}  // namespace internal
}  // namespace ash
