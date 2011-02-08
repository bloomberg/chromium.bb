// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_background.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/view.h"

// static
const int InfoBarBackground::kSeparatorLineHeight = 1;

InfoBarBackground::InfoBarBackground(InfoBarDelegate::Type infobar_type) {
  gradient_background_.reset(
      views::Background::CreateVerticalGradientBackground(
          GetTopColor(infobar_type),
          GetBottomColor(infobar_type)));
}

InfoBarBackground::~InfoBarBackground() {
}

SkColor InfoBarBackground::GetTopColor(InfoBarDelegate::Type infobar_type) {
  static const SkColor kWarningBackgroundColorTop =
      SkColorSetRGB(255, 242, 183);
  static const SkColor kPageActionBackgroundColorTop =
      SkColorSetRGB(218, 231, 249);

  return (infobar_type == InfoBarDelegate::WARNING_TYPE) ?
      kWarningBackgroundColorTop : kPageActionBackgroundColorTop;
}

SkColor InfoBarBackground::GetBottomColor(InfoBarDelegate::Type infobar_type) {
  static const SkColor kWarningBackgroundColorBottom =
      SkColorSetRGB(250, 230, 145);
  static const SkColor kPageActionBackgroundColorBottom =
      SkColorSetRGB(179, 202, 231);

  return (infobar_type == InfoBarDelegate::WARNING_TYPE) ?
      kWarningBackgroundColorBottom : kPageActionBackgroundColorBottom;
}

void InfoBarBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  gradient_background_->Paint(canvas, view);
  canvas->FillRectInt(ResourceBundle::toolbar_separator_color, 0,
                      view->height() - kSeparatorLineHeight, view->width(),
                      kSeparatorLineHeight);
}
