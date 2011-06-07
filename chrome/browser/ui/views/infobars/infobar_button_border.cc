// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_button_border.h"

#include "grit/theme_resources.h"
#include "ui/base/animation/throb_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"

InfoBarButtonBorder::InfoBarButtonBorder() {
#ifdef TOUCH_UI
  static const int kPreferredPaddingVertical = 12;
  set_vertical_padding(kPreferredPaddingVertical);
#endif

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  BorderImageSet normal_set = {
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_LEFT_N),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_N),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_RIGHT_N),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_LEFT_N),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_CENTER_N),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_RIGHT_N),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_N),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_N),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_RIGHT_N),
  };
  set_normal_set(normal_set);

  BorderImageSet hot_set = {
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_LEFT_H),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_H),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_RIGHT_H),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_LEFT_H),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_CENTER_H),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_RIGHT_H),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_H),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_H),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_RIGHT_H),
  };
  set_hot_set(hot_set);

  BorderImageSet pushed_set = {
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_LEFT_P),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_P),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_RIGHT_P),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_LEFT_P),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_CENTER_P),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_RIGHT_P),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_P),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_P),
    rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_RIGHT_P),
  };
  set_pushed_set(pushed_set);
}

InfoBarButtonBorder::~InfoBarButtonBorder() {
}
