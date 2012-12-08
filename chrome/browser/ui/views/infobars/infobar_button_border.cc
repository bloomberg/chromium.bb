// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_button_border.h"

#include "chrome/browser/defaults.h"
#include "grit/theme_resources.h"
#include "ui/views/painter.h"

namespace {

const int kNormalImageSet[] = IMAGE_GRID(IDR_INFOBARBUTTON_NORMAL);
const int kHotImageSet[] = IMAGE_GRID(IDR_INFOBARBUTTON_HOVER);
const int kPushedImageSet[] = IMAGE_GRID(IDR_INFOBARBUTTON_PRESSED);

}  // namespace

InfoBarButtonBorder::InfoBarButtonBorder() {
  gfx::Insets insets = GetInsets();
  SetInsets(gfx::Insets(browser_defaults::kInfoBarBorderPaddingVertical,
                        insets.left(),
                        browser_defaults::kInfoBarBorderPaddingVertical,
                        insets.right()));

  set_normal_painter(views::Painter::CreateImageGridPainter(kNormalImageSet));
  set_hot_painter(views::Painter::CreateImageGridPainter(kHotImageSet));
  set_pushed_painter(views::Painter::CreateImageGridPainter(kPushedImageSet));
}

InfoBarButtonBorder::~InfoBarButtonBorder() {
}
