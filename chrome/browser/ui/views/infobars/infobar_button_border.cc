// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_button_border.h"

#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/controls/button/text_button.h"

// Preferred padding between text and edge
static const int kPreferredPaddingHorizontal = 6;
static const int kPreferredPaddingVertical = 5;

// InfoBarButtonBorder, public: ----------------------------------------------

InfoBarButtonBorder::InfoBarButtonBorder() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  normal_set_.top_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_LEFT_N);
  normal_set_.top = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_N);
  normal_set_.top_right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_RIGHT_N);
  normal_set_.left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_LEFT_N);
  normal_set_.center = rb.GetBitmapNamed(IDR_INFOBARBUTTON_CENTER_N);
  normal_set_.right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_RIGHT_N);
  normal_set_.bottom_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_N);
  normal_set_.bottom = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_N);
  normal_set_.bottom_right =
      rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_RIGHT_N);

  hot_set_.top_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_LEFT_H);
  hot_set_.top = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_H);
  hot_set_.top_right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_RIGHT_H);
  hot_set_.left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_LEFT_H);
  hot_set_.center = rb.GetBitmapNamed(IDR_INFOBARBUTTON_CENTER_H);
  hot_set_.right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_RIGHT_H);
  hot_set_.bottom_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_H);
  hot_set_.bottom = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_H);
  hot_set_.bottom_right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_RIGHT_H);

  pushed_set_.top_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_LEFT_P);
  pushed_set_.top = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_P);
  pushed_set_.top_right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_RIGHT_P);
  pushed_set_.left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_LEFT_P);
  pushed_set_.center = rb.GetBitmapNamed(IDR_INFOBARBUTTON_CENTER_P);
  pushed_set_.right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_RIGHT_P);
  pushed_set_.bottom_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_P);
  pushed_set_.bottom = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_P);
  pushed_set_.bottom_right =
      rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_RIGHT_P);
}

InfoBarButtonBorder::~InfoBarButtonBorder() {
}

// InfoBarButtonBorder, Border overrides: ------------------------------------

void InfoBarButtonBorder::GetInsets(gfx::Insets* insets) const {
  insets->Set(kPreferredPaddingVertical, kPreferredPaddingHorizontal,
              kPreferredPaddingVertical, kPreferredPaddingHorizontal);
}

void InfoBarButtonBorder::Paint(const views::View& view,
                                gfx::Canvas* canvas) const {
  const views::TextButton* mb = static_cast<const views::TextButton*>(&view);
  int state = mb->state();

  // TextButton takes care of deciding when to call Paint.
  const MBBImageSet* set = &normal_set_;
  if (state == views::TextButton::BS_HOT)
    set = &hot_set_;
  else if (state == views::TextButton::BS_PUSHED)
    set = &pushed_set_;

  gfx::Rect bounds = view.bounds();

  // Draw top left image.
  canvas->DrawBitmapInt(*set->top_left, 0, 0);

  // Stretch top image.
  canvas->DrawBitmapInt(
      *set->top,
      0, 0, set->top->width(), set->top->height(),
      set->top_left->width(),
      0,
      bounds.width() - set->top_right->width() - set->top_left->width(),
      set->top->height(), false);

  // Draw top right image.
  canvas->DrawBitmapInt(*set->top_right,
                        bounds.width() - set->top_right->width(), 0);

  // Stretch left image.
  canvas->DrawBitmapInt(
      *set->left,
      0, 0, set->left->width(), set->left->height(),
      0,
      set->top_left->height(),
      set->top_left->width(),
      bounds.height() - set->top->height() - set->bottom_left->height(), false);

  // Stretch center image.
  canvas->DrawBitmapInt(
      *set->center,
      0, 0, set->center->width(), set->center->height(),
      set->left->width(),
      set->top->height(),
      bounds.width() - set->right->width() - set->left->width(),
      bounds.height() - set->bottom->height() - set->top->height(), false);

  // Stretch right image.
  canvas->DrawBitmapInt(
      *set->right,
      0, 0, set->right->width(), set->right->height(),
      bounds.width() - set->right->width(),
      set->top_right->height(),
      set->right->width(),
      bounds.height() - set->bottom_right->height() -
          set->top_right->height(), false);

  // Draw bottom left image.
  canvas->DrawBitmapInt(*set->bottom_left,
                        0,
                        bounds.height() - set->bottom_left->height());

  // Stretch bottom image.
  canvas->DrawBitmapInt(
      *set->bottom,
      0, 0, set->bottom->width(), set->bottom->height(),
      set->bottom_left->width(),
      bounds.height() - set->bottom->height(),
      bounds.width() - set->bottom_right->width() -
          set->bottom_left->width(),
      set->bottom->height(), false);

  // Draw bottom right image.
  canvas->DrawBitmapInt(*set->bottom_right,
                        bounds.width() - set->bottom_right->width(),
                        bounds.height() -  set->bottom_right->height());
}
