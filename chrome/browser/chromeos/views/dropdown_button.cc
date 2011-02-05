// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/views/dropdown_button.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"

namespace {
// Asset icon particularities makes us offset focus frame.
const int kFocusFrameTopOffset = 0;
const int kFocusFrameLeftOffset = 0;
const int kFocusFrameRightOffset = 0;
const int kFocusFrameBottomOffset = 1;

// TextButtonBorder specification that uses different icons to draw the
// button.
class DropDownButtonBorder : public views::TextButtonBorder {
 public:
  DropDownButtonBorder();

 private:
  DISALLOW_COPY_AND_ASSIGN(DropDownButtonBorder);
};

DropDownButtonBorder::DropDownButtonBorder() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  hot_set_.top_left = rb.GetBitmapNamed(IDR_DROPDOWN_TOP_LEFT_H);
  hot_set_.top = rb.GetBitmapNamed(IDR_DROPDOWN_TOP_H);
  hot_set_.top_right = rb.GetBitmapNamed(IDR_DROPDOWN_TOP_RIGHT_H);
  hot_set_.left = rb.GetBitmapNamed(IDR_DROPDOWN_LEFT_H);
  hot_set_.center = rb.GetBitmapNamed(IDR_DROPDOWN_CENTER_H);
  hot_set_.right = rb.GetBitmapNamed(IDR_DROPDOWN_RIGHT_H);
  hot_set_.bottom_left = rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_LEFT_H);
  hot_set_.bottom = rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_H);
  hot_set_.bottom_right = rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_RIGHT_H);

  pushed_set_.top_left = rb.GetBitmapNamed(IDR_DROPDOWN_TOP_LEFT_P);
  pushed_set_.top = rb.GetBitmapNamed(IDR_DROPDOWN_TOP_P);
  pushed_set_.top_right = rb.GetBitmapNamed(IDR_DROPDOWN_TOP_RIGHT_P);
  pushed_set_.left = rb.GetBitmapNamed(IDR_DROPDOWN_LEFT_P);
  pushed_set_.center = rb.GetBitmapNamed(IDR_DROPDOWN_CENTER_P);
  pushed_set_.right = rb.GetBitmapNamed(IDR_DROPDOWN_RIGHT_P);
  pushed_set_.bottom_left = rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_LEFT_P);
  pushed_set_.bottom = rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_P);
  pushed_set_.bottom_right = rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_RIGHT_P);
}

}  // namespace

namespace chromeos {

DropDownButton::DropDownButton(views::ButtonListener* listener,
                               const std::wstring& text,
                               views::ViewMenuDelegate* menu_delegate,
                               bool show_menu_marker)
    : MenuButton(listener, text, menu_delegate, show_menu_marker) {
  set_border(new DropDownButtonBorder);
}

DropDownButton::~DropDownButton() {
}

void DropDownButton::PaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (IsFocusable() || IsAccessibilityFocusableInRootView()))
    canvas->DrawFocusRect(kFocusFrameLeftOffset, kFocusFrameTopOffset,
                          width() - kFocusFrameRightOffset,
                          height() - kFocusFrameBottomOffset);
}

}  // namespace chromeos
