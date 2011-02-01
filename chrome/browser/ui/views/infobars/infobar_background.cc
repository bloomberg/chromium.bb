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
  SkColor top_color;
  SkColor bottom_color;
  switch (infobar_type) {
    case InfoBarDelegate::WARNING_TYPE:
      top_color = kWarningBackgroundColorTop;
      bottom_color = kWarningBackgroundColorBottom;
      break;
    case InfoBarDelegate::PAGE_ACTION_TYPE:
      top_color = kPageActionBackgroundColorTop;
      bottom_color = kPageActionBackgroundColorBottom;
      break;
    default:
      NOTREACHED();
      break;
  }
  gradient_background_.reset(
      views::Background::CreateVerticalGradientBackground(top_color,
                                                          bottom_color));
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
