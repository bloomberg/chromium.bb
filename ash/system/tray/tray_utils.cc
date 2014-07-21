// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_utils.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ui/gfx/font_list.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace ash {

void SetupLabelForTray(views::Label* label) {
  label->SetFontList(gfx::FontList().Derive(1, gfx::Font::BOLD));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(SK_ColorWHITE);
  label->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
  label->SetShadows(gfx::ShadowValues(
      1, gfx::ShadowValue(gfx::Point(0, 1), 0, SkColorSetARGB(64, 0, 0, 0))));
}

void SetTrayImageItemBorder(views::View* tray_view,
                            ShelfAlignment alignment) {
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_TOP) {
    tray_view->SetBorder(views::Border::CreateEmptyBorder(
        0,
        kTrayImageItemHorizontalPaddingBottomAlignment,
        0,
        kTrayImageItemHorizontalPaddingBottomAlignment));
  } else {
    tray_view->SetBorder(views::Border::CreateEmptyBorder(
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
    tray_view->SetBorder(views::Border::CreateEmptyBorder(
        0,
        kTrayLabelItemHorizontalPaddingBottomAlignment,
        0,
        kTrayLabelItemHorizontalPaddingBottomAlignment));
  } else {
    // Center the label for vertical launcher alignment.
    int horizontal_padding = std::max(0,
        (tray_view->GetPreferredSize().width() -
        tray_view->label()->GetPreferredSize().width()) / 2);
    tray_view->SetBorder(views::Border::CreateEmptyBorder(
        kTrayLabelItemVerticalPaddingVerticalAlignment,
        horizontal_padding,
        kTrayLabelItemVerticalPaddingVerticalAlignment,
        horizontal_padding));
  }
}

}  // namespace ash
