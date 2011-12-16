// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  BorderImageSet hot_set = {
    rb.GetBitmapNamed(IDR_DROPDOWN_TOP_LEFT_H),
    rb.GetBitmapNamed(IDR_DROPDOWN_TOP_H),
    rb.GetBitmapNamed(IDR_DROPDOWN_TOP_RIGHT_H),
    rb.GetBitmapNamed(IDR_DROPDOWN_LEFT_H),
    rb.GetBitmapNamed(IDR_DROPDOWN_CENTER_H),
    rb.GetBitmapNamed(IDR_DROPDOWN_RIGHT_H),
    rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_LEFT_H),
    rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_H),
    rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_RIGHT_H),
  };
  set_hot_set(hot_set);

  BorderImageSet pushed_set = {
    rb.GetBitmapNamed(IDR_DROPDOWN_TOP_LEFT_P),
    rb.GetBitmapNamed(IDR_DROPDOWN_TOP_P),
    rb.GetBitmapNamed(IDR_DROPDOWN_TOP_RIGHT_P),
    rb.GetBitmapNamed(IDR_DROPDOWN_LEFT_P),
    rb.GetBitmapNamed(IDR_DROPDOWN_CENTER_P),
    rb.GetBitmapNamed(IDR_DROPDOWN_RIGHT_P),
    rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_LEFT_P),
    rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_P),
    rb.GetBitmapNamed(IDR_DROPDOWN_BOTTOM_RIGHT_P),
  };
  set_pushed_set(pushed_set);
}

}  // namespace

namespace chromeos {

DropDownButton::DropDownButton(views::ButtonListener* listener,
                               const string16& text,
                               views::ViewMenuDelegate* menu_delegate,
                               bool show_menu_marker)
    : MenuButton(listener, text, menu_delegate, show_menu_marker) {
  set_border(new DropDownButtonBorder);
}

DropDownButton::~DropDownButton() {
}

void DropDownButton::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusableInRootView())) {
    canvas->DrawFocusRect(gfx::Rect(kFocusFrameLeftOffset, kFocusFrameTopOffset,
                                    width() - kFocusFrameRightOffset,
                                    height() - kFocusFrameBottomOffset));
  }
}

void DropDownButton::SetText(const string16& text) {
  text_ = text;
  UpdateTextSize();
}

string16 DropDownButton::GetAccessibleValue() {
  return text_;
}

}  // namespace chromeos
