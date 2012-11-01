// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_button_border.h"

#include "chrome/browser/defaults.h"
#include "grit/theme_resources.h"
#include "ui/views/controls/button/border_images.h"

namespace {

const int kNormalImageSet[] = { BORDER_IMAGES(IDR_INFOBARBUTTON_NORMAL) };
const int kHotImageSet[] = { BORDER_IMAGES(IDR_INFOBARBUTTON_HOVER) };
const int kPushedImageSet[] = { BORDER_IMAGES(IDR_INFOBARBUTTON_PRESSED) };

}  // namespace

InfoBarButtonBorder::InfoBarButtonBorder() {
  set_vertical_padding(browser_defaults::kInfoBarBorderPaddingVertical);
  set_normal_set(views::BorderImages(kNormalImageSet));
  set_hot_set(views::BorderImages(kHotImageSet));
  set_pushed_set(views::BorderImages(kPushedImageSet));
}

InfoBarButtonBorder::~InfoBarButtonBorder() {
}
