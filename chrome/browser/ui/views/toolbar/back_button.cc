// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/back_button.h"

#include "ui/gfx/insets.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/painter.h"

BackButton::BackButton(views::ButtonListener* listener,
                       ui::MenuModel* model)
    : ToolbarButton(listener, model),
      margin_leading_(0) {
}

BackButton::~BackButton() {
}

gfx::Rect BackButton::GetThemePaintRect() const  {
  gfx::Rect rect(LabelButton::GetThemePaintRect());
  rect.Inset(margin_leading_, 0, 0, 0);
  return rect;
}

void BackButton::SetLeadingMargin(int margin) {
  // Adjust border insets to follow the margin change,
  // which will be reflected in where the border is painted
  // through |GetThemePaintRect|.
  scoped_ptr<views::LabelButtonBorder> border(
      new views::LabelButtonBorder(style()));
  const gfx::Insets insets(border->GetInsets());
  border->set_insets(gfx::Insets(insets.top(), insets.left() + margin,
                                 insets.bottom(), insets.right()));
  UpdateThemedBorder(border.PassAs<views::Border>());

  // Similarly fiddle the focus border. Value consistent with LabelButton
  // and TextButton.
  // TODO(gbillock): Refactor this magic number somewhere global to views,
  // probably a FocusBorder constant.
  const int kFocusRectInset = 3;
  SetFocusPainter(views::Painter::CreateDashedFocusPainterWithInsets(
                      gfx::Insets(kFocusRectInset, kFocusRectInset + margin,
                                  kFocusRectInset, kFocusRectInset)));

  margin_leading_ = margin;
  InvalidateLayout();
}
