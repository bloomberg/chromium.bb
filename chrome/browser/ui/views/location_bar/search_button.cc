// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/search_button.h"

#include "grit/theme_resources.h"
#include "ui/base/theme_provider.h"
#include "ui/views/controls/button/label_button_border.h"

SearchButton::SearchButton(views::ButtonListener* listener)
    : views::LabelButton(listener, base::string16()) {
  EnableCanvasFlippingForRTLUI(true);
  set_triggerable_event_flags(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON);
  SetStyle(views::Button::STYLE_BUTTON);
  SetFocusable(false);
  SetMinSize(gfx::Size());
  scoped_ptr<views::LabelButtonBorder> border(
      new views::LabelButtonBorder(style()));
  border->set_insets(gfx::Insets());
  const int kSearchButtonNormalImages[] = IMAGE_GRID(IDR_OMNIBOX_SEARCH_BUTTON);
  border->SetPainter(
      false, views::Button::STATE_NORMAL,
      views::Painter::CreateImageGridPainter(kSearchButtonNormalImages));
  const int kSearchButtonHoveredImages[] =
      IMAGE_GRID(IDR_OMNIBOX_SEARCH_BUTTON_HOVER);
  border->SetPainter(
      false, views::Button::STATE_HOVERED,
      views::Painter::CreateImageGridPainter(kSearchButtonHoveredImages));
  const int kSearchButtonPressedImages[] =
      IMAGE_GRID(IDR_OMNIBOX_SEARCH_BUTTON_PRESSED);
  border->SetPainter(
      false, views::Button::STATE_PRESSED,
      views::Painter::CreateImageGridPainter(kSearchButtonPressedImages));
  border->SetPainter(false, views::Button::STATE_DISABLED, NULL);
  border->SetPainter(true, views::Button::STATE_NORMAL, NULL);
  border->SetPainter(true, views::Button::STATE_HOVERED, NULL);
  border->SetPainter(true, views::Button::STATE_PRESSED, NULL);
  border->SetPainter(true, views::Button::STATE_DISABLED, NULL);
  SetBorder(border.PassAs<views::Border>());
  const int kSearchButtonWidth = 56;
  SetMinSize(gfx::Size(kSearchButtonWidth, 0));
}

SearchButton::~SearchButton() {
}

void SearchButton::UpdateIcon(bool is_search) {
  SetImage(
      views::Button::STATE_NORMAL,
      *GetThemeProvider()->GetImageSkiaNamed(is_search ?
          IDR_OMNIBOX_SEARCH_BUTTON_LOUPE : IDR_OMNIBOX_SEARCH_BUTTON_ARROW));
  // Flip the arrow for RTL, but not the loupe.
  image()->EnableCanvasFlippingForRTLUI(!is_search);
}
