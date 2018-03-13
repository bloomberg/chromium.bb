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

gfx::Size OmniboxTabSwitchButton::CalculatePreferredSize() const {
  gfx::Size size = MdTextButton::CalculatePreferredSize();
  const int horizontal_padding =
      GetLayoutConstant(LOCATION_BAR_PADDING) +
      GetLayoutConstant(LOCATION_BAR_ICON_INTERIOR_PADDING);
  size.set_width(size.width() + horizontal_padding);
  return size;
}

void OmniboxTabSwitchButton::StateChanged(ButtonState old_state) {
  if (state() == STATE_NORMAL) {
    SetBgColorOverride(GetBackgroundColor());
    // If used to be pressed, transer ownership.
    if (old_state == STATE_PRESSED)
      SetMouseHandler(result_view_);
  }
  if (state() == STATE_HOVERED) {
    if (old_state == STATE_NORMAL) {
      SetBgColorOverride(GetBackgroundColor());
    } else {
      // The button was released.
      result_view_->OpenMatch(WindowOpenDisposition::SWITCH_TO_TAB);
    }
  }
  if (state() == STATE_PRESSED)
    SetPressed();
  LabelButton::StateChanged(old_state);
}

SkColor OmniboxTabSwitchButton::GetBackgroundColor() const {
  return GetOmniboxColor(OmniboxPart::RESULTS_BACKGROUND,
                         result_view_->GetTint(),
                         state() == STATE_HOVERED ? OmniboxPartState::HOVERED
                                                  : OmniboxPartState::NORMAL);
}

void OmniboxTabSwitchButton::SetPressed() {
  SetBgColorOverride(color_utils::AlphaBlend(
      GetOmniboxColor(OmniboxPart::RESULTS_BACKGROUND, result_view_->GetTint(),
                      OmniboxPartState::SELECTED),
      SK_ColorBLACK, 0.8 * 255));
}
