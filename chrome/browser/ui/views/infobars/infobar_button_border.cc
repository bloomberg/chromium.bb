// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_button_border.h"

#include "chrome/browser/defaults.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

InfoBarButtonBorder::InfoBarButtonBorder() {
  set_vertical_padding(browser_defaults::kInfoBarBorderPaddingVertical);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  BorderImageSet normal_set = {
    rb.GetImageNamed(IDR_INFOBARBUTTON_TOP_LEFT_N).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_TOP_N).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_TOP_RIGHT_N).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_LEFT_N).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_CENTER_N).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_RIGHT_N).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_N).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_BOTTOM_N).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_BOTTOM_RIGHT_N).ToImageSkia(),
  };
  set_normal_set(normal_set);

  BorderImageSet hot_set = {
    rb.GetImageNamed(IDR_INFOBARBUTTON_TOP_LEFT_H).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_TOP_H).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_TOP_RIGHT_H).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_LEFT_H).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_CENTER_H).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_RIGHT_H).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_H).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_BOTTOM_H).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_BOTTOM_RIGHT_H).ToImageSkia(),
  };
  set_hot_set(hot_set);

  BorderImageSet pushed_set = {
    rb.GetImageNamed(IDR_INFOBARBUTTON_TOP_LEFT_P).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_TOP_P).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_TOP_RIGHT_P).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_LEFT_P).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_CENTER_P).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_RIGHT_P).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_P).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_BOTTOM_P).ToImageSkia(),
    rb.GetImageNamed(IDR_INFOBARBUTTON_BOTTOM_RIGHT_P).ToImageSkia(),
  };
  set_pushed_set(pushed_set);
}

InfoBarButtonBorder::~InfoBarButtonBorder() {
}
