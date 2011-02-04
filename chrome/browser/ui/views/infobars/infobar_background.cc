// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_background.h"

#include "gfx/canvas.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/view.h"

static const SkColor kWarningBackgroundColorTop = SkColorSetRGB(255, 242, 183);
static const SkColor kWarningBackgroundColorBottom =
    SkColorSetRGB(250, 230, 145);

static const SkColor kPageActionBackgroundColorTop =
    SkColorSetRGB(218, 231, 249);
static const SkColor kPageActionBackgroundColorBottom =
    SkColorSetRGB(179, 202, 231);

static const int kSeparatorLineHeight = 1;

// InfoBarBackground, public: --------------------------------------------------

InfoBarBackground::InfoBarBackground(InfoBarDelegate::Type infobar_type) {
  gradient_background_.reset(
      views::Background::CreateVerticalGradientBackground(
          GetTopColor(infobar_type),
          GetBottomColor(infobar_type)));
}

SkColor InfoBarBackground::GetTopColor(InfoBarDelegate::Type infobar_type) {
  return (infobar_type == InfoBarDelegate::WARNING_TYPE) ?
      kWarningBackgroundColorTop : kPageActionBackgroundColorTop;
}

SkColor InfoBarBackground::GetBottomColor(InfoBarDelegate::Type infobar_type) {
  return (infobar_type == InfoBarDelegate::WARNING_TYPE) ?
      kWarningBackgroundColorBottom : kPageActionBackgroundColorBottom;
}

// InfoBarBackground, views::Background overrides: -----------------------------

void InfoBarBackground::Paint(gfx::Canvas* canvas, views::View* view) const {
  // First paint the gradient background.
  gradient_background_->Paint(canvas, view);

  // Now paint the separator line.
  canvas->FillRectInt(ResourceBundle::toolbar_separator_color, 0,
                      view->height() - kSeparatorLineHeight, view->width(),
                      kSeparatorLineHeight);
}
