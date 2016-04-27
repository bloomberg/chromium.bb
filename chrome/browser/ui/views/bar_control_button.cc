// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bar_control_button.h"

#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/animation/button_ink_drop_delegate.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace {

// Extra space around the buttons to increase their event target size.
const int kButtonExtraTouchSize = 4;

}  // namespace

BarControlButton::BarControlButton(views::ButtonListener* listener)
    : views::ImageButton(listener),
      id_(gfx::VectorIconId::VECTOR_ICON_NONE),
      ink_drop_delegate_(new views::ButtonInkDropDelegate(this, this)) {
  set_ink_drop_delegate(ink_drop_delegate_.get());
  set_has_ink_drop_action_on_click(true);
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  SetFocusPainter(nullptr);
  UseMdFocusRing();
}

BarControlButton::~BarControlButton() {}

void BarControlButton::SetIcon(
    gfx::VectorIconId id,
    const base::Callback<SkColor(void)>& get_text_color_callback) {
  id_ = id;
  get_text_color_callback_ = get_text_color_callback;

  if (!border()) {
    SetBorder(views::Border::CreateEmptyBorder(
        kButtonExtraTouchSize, kButtonExtraTouchSize, kButtonExtraTouchSize,
        kButtonExtraTouchSize));
  }
}

void BarControlButton::OnThemeChanged() {
  SkColor icon_color =
      color_utils::DeriveDefaultIconColor(get_text_color_callback_.Run());
  gfx::ImageSkia image = gfx::CreateVectorIcon(id_, icon_color);
  SetImage(views::CustomButton::STATE_NORMAL, &image);
  image = gfx::CreateVectorIcon(id_, SkColorSetA(icon_color, 0xff / 2));
  SetImage(views::CustomButton::STATE_DISABLED, &image);
  set_ink_drop_base_color(icon_color);
}

void BarControlButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  OnThemeChanged();
}
