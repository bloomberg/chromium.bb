// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_tab_switch_button.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/label_button_border.h"

OmniboxTabSwitchButton::OmniboxTabSwitchButton(OmniboxResultView* result_view)
    : MdTextButton(this, views::style::CONTEXT_BUTTON_MD),
      result_view_(result_view) {
  // TODO: SetTooltipText(text);
  //       SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
  SetBackground(std::make_unique<BackgroundWith1PxBorder>(GetBackgroundColor(),
                                                          SK_ColorBLACK));
  SetBgColorOverride(GetBackgroundColor());
  SetImage(STATE_NORMAL,
           gfx::CreateVectorIcon(omnibox::kSwitchIcon, 16, SK_ColorBLACK));
  SetText(base::ASCIIToUTF16("Switch to open tab"));
}

void OmniboxTabSwitchButton::SetPressed() {
  SetBgColorOverride(color_utils::AlphaBlend(
      GetOmniboxColor(OmniboxPart::RESULTS_BACKGROUND, result_view_->GetTint(),
                      OmniboxPartState::SELECTED),
      SK_ColorBLACK, 0.8 * 255));
}

void OmniboxTabSwitchButton::ClearState() {
  SetBgColorOverride(GetBackgroundColor());
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
  gfx::Size size = MdTextButton::CalculatePreferredSize();
  const int horizontal_padding =
      GetLayoutConstant(LOCATION_BAR_PADDING) +
      GetLayoutConstant(LOCATION_BAR_ICON_INTERIOR_PADDING);
  size.set_width(size.width() + horizontal_padding);
  return size;
}

void OmniboxTabSwitchButton::StateChanged(ButtonState old_state) {
  SetBgColorOverride(GetBackgroundColor());
  MdTextButton::StateChanged(old_state);
}

SkColor OmniboxTabSwitchButton::GetBackgroundColor() const {
  return GetOmniboxColor(OmniboxPart::RESULTS_BACKGROUND,
                         result_view_->GetTint(),
                         state() == STATE_HOVERED ? OmniboxPartState::HOVERED
                                                  : OmniboxPartState::NORMAL);
}
