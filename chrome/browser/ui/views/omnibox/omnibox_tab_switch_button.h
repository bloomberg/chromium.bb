// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/location_bar/background_with_1_px_border.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"

class OmniboxTabSwitchButton : public views::LabelButton,
                               views::ButtonListener {
 public:
  explicit OmniboxTabSwitchButton(OmniboxResultView* result_view)
      : LabelButton(this, base::ASCIIToUTF16("Switch tabs")),
        result_view_(result_view) {
    // TODO: SetTooltipText(text);
    //       SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
    const SkColor bg_color = result_view_->GetColor(
        OmniboxResultView::NORMAL, OmniboxResultView::BACKGROUND);
    SetBackground(
        std::make_unique<BackgroundWith1PxBorder>(bg_color, SK_ColorBLACK));
    SetImage(STATE_NORMAL,
             gfx::CreateVectorIcon(omnibox::kTabIcon, 16, SK_ColorBLACK));
  }

  void SetPressed();
  void ClearState();

  // views::View
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener
  void ButtonPressed(Button* sender, const ui::Event& event) override {}

  // views::Button
  void StateChanged(ButtonState old_state) override;

 private:
  OmniboxResultView* result_view_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxTabSwitchButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
