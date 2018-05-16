// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_

#include "ui/views/controls/button/md_text_button.h"

class OmniboxPopupContentsView;
class OmniboxResultView;

class OmniboxTabSwitchButton : public views::MdTextButton {
 public:
  OmniboxTabSwitchButton(OmniboxPopupContentsView* model,
                         OmniboxResultView* result_view,
                         int text_height);

  // views::View
  gfx::Size CalculatePreferredSize() const override;

  // views::Button
  void StateChanged(ButtonState old_state) override;

  // Called by parent views to change background on external (not mouse related)
  // event (tab key).
  void UpdateBackground();

 private:
  // Encapsulates the color look-up, which uses the button state (hovered,
  // etc.) and consults the parent result view.
  SkColor GetBackgroundColor() const;

  // Encapsulates changing the color of the button to display being
  // pressed.
  void SetPressed();

  static constexpr int kVerticalPadding = 3;
  const int text_height_;
  OmniboxPopupContentsView* model_;
  OmniboxResultView* result_view_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxTabSwitchButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
