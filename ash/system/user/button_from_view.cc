// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/button_from_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// The border color of the user button.
const SkColor kBorderColor = 0xffdcdcdc;

}  // namespace

namespace tray {

ButtonFromView::ButtonFromView(views::View* content,
                               views::ButtonListener* listener,
                               bool highlight_on_hover)
    : CustomButton(listener),
      content_(content),
      highlight_on_hover_(highlight_on_hover),
      button_hovered_(false),
      show_border_(false) {
  set_notify_enter_exit_on_child(true);
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  AddChildView(content_);
  ShowActive();
}

ButtonFromView::~ButtonFromView() {}

void ButtonFromView::ForceBorderVisible(bool show) {
  show_border_ = show;
  ShowActive();
}

void ButtonFromView::OnMouseEntered(const ui::MouseEvent& event) {
  button_hovered_ = true;
  ShowActive();
}

void ButtonFromView::OnMouseExited(const ui::MouseEvent& event) {
  button_hovered_ = false;
  ShowActive();
}

void ButtonFromView::ShowActive() {
  bool border_visible =
      (button_hovered_ && highlight_on_hover_) || show_border_;
  SkColor border_color = border_visible ? kBorderColor : SK_ColorTRANSPARENT;
  SetBorder(views::Border::CreateSolidBorder(1, border_color));
  if (highlight_on_hover_) {
    SkColor background_color =
        button_hovered_ ? kHoverBackgroundColor : kBackgroundColor;
    content_->set_background(
        views::Background::CreateSolidBackground(background_color));
    set_background(views::Background::CreateSolidBackground(background_color));
  }
  SchedulePaint();
}

}  // namespace tray
}  // namespace ash
