// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_tab_switch_button.h"

#include "chrome/browser/ui/layout_constants.h"
#include "ui/gfx/color_utils.h"

void OmniboxTabSwitchButton::SetPressed() {
  const SkColor bg_color = result_view_->GetColor(
      OmniboxResultView::SELECTED, OmniboxResultView::BACKGROUND);
  // Using transparent does nothing, since the result view is also selected.
  const SkColor pressed_color =
      color_utils::AlphaBlend(bg_color, SK_ColorBLACK, 0.8 * 255);
  SetBackground(
      std::make_unique<BackgroundWith1PxBorder>(pressed_color, SK_ColorBLACK));
  SchedulePaint();
}

void OmniboxTabSwitchButton::ClearState() {
  // TODO: Consider giving this a non-transparent background.
  SetBackground(nullptr);
  SchedulePaint();
}

bool OmniboxTabSwitchButton::OnMousePressed(const ui::MouseEvent& event) {
  SetPressed();

  // We return true to tell caller, "We want drag events."
  return true;
}

bool OmniboxTabSwitchButton::OnMouseDragged(const ui::MouseEvent& event) {
  if (HitTestPoint(event.location())) {
    // TODO: Only do this on the first movement.
    SetPressed();
    // I don't think this has any effect.
    return true;
  } else {
    ClearState();
    SetMouseHandler(result_view_);
    return false;
  }
}

void OmniboxTabSwitchButton::OnMouseReleased(const ui::MouseEvent& event) {
  // We're not going to be called again.
  ClearState();
  const ui::MouseEvent* mouse = event.AsMouseEvent();
  if (mouse->IsOnlyLeftMouseButton())
    result_view_->OpenMatch(WindowOpenDisposition::SWITCH_TO_TAB);
}

gfx::Size OmniboxTabSwitchButton::CalculatePreferredSize() const {
  gfx::Size size = LabelButton::CalculatePreferredSize();
  const int horizontal_padding =
      GetLayoutConstant(LOCATION_BAR_PADDING) +
      GetLayoutConstant(LOCATION_BAR_ICON_INTERIOR_PADDING);
  size.set_width(size.width() + horizontal_padding);
  return size;
}

void OmniboxTabSwitchButton::StateChanged(ButtonState old_state) {
  const SkColor bg_color = result_view_->GetColor(
      state() == STATE_HOVERED ? OmniboxResultView::HOVERED
                               : OmniboxResultView::NORMAL,
      OmniboxResultView::BACKGROUND);
  SetBackground(
      std::make_unique<BackgroundWith1PxBorder>(bg_color, SK_ColorBLACK));
  LabelButton::StateChanged(old_state);
}
