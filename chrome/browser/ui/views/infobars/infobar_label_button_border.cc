// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_label_button_border.h"

#include "chrome/browser/defaults.h"
#include "grit/theme_resources.h"
#include "ui/views/painter.h"

namespace {

const int kNormalImageSet[] = IMAGE_GRID(IDR_INFOBARBUTTON_NORMAL);
const int kHoveredImageSet[] = IMAGE_GRID(IDR_INFOBARBUTTON_HOVER);
const int kPressedImageSet[] = IMAGE_GRID(IDR_INFOBARBUTTON_PRESSED);

}  // namespace

InfoBarLabelButtonBorder::InfoBarLabelButtonBorder()
    : views::LabelButtonBorder(views::Button::STYLE_TEXTBUTTON) {
  SetPainter(false, views::Button::STATE_NORMAL,
             views::Painter::CreateImageGridPainter(kNormalImageSet));
  SetPainter(false, views::Button::STATE_HOVERED,
             views::Painter::CreateImageGridPainter(kHoveredImageSet));
  SetPainter(false, views::Button::STATE_PRESSED,
             views::Painter::CreateImageGridPainter(kPressedImageSet));
}

InfoBarLabelButtonBorder::~InfoBarLabelButtonBorder() {
}

gfx::Insets InfoBarLabelButtonBorder::GetInsets() const{
  gfx::Insets insets = views::LabelButtonBorder::GetInsets();
  return gfx::Insets(browser_defaults::kInfoBarBorderPaddingVertical,
                     insets.left(),
                     browser_defaults::kInfoBarBorderPaddingVertical,
                     insets.right());
}
