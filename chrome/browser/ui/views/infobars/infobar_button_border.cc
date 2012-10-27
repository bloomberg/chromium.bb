// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_button_border.h"

#include "chrome/browser/defaults.h"
#include "grit/theme_resources.h"
#include "ui/views/controls/button/border_images.h"

#define BORDER_IMAGES(x, y) \
  x ## _TOP_LEFT_ ## y,    x ## _TOP_ ## y,    x ## _TOP_RIGHT_ ## y, \
  x ## _LEFT_ ## y,        x ## _CENTER_ ## y, x ## _RIGHT_ ## y, \
  x ## _BOTTOM_LEFT_ ## y, x ## _BOTTOM_ ## y, x ## _BOTTOM_RIGHT_ ## y,

namespace {

const int kNormalImageSet[] = { BORDER_IMAGES(IDR_INFOBARBUTTON, N) };
const int kHotImageSet[] = { BORDER_IMAGES(IDR_INFOBARBUTTON, H) };
const int kPushedImageSet[] = { BORDER_IMAGES(IDR_INFOBARBUTTON, P) };

}  // namespace

InfoBarButtonBorder::InfoBarButtonBorder() {
  set_vertical_padding(browser_defaults::kInfoBarBorderPaddingVertical);
  set_normal_set(views::BorderImages(kNormalImageSet));
  set_hot_set(views::BorderImages(kHotImageSet));
  set_pushed_set(views::BorderImages(kPushedImageSet));
}

InfoBarButtonBorder::~InfoBarButtonBorder() {
}
