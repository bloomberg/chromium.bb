// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_

#include "ui/views/controls/button/md_text_button.h"

class OmniboxResultView;

class OmniboxTabSwitchButton : public views::MdTextButton,
                               public views::ButtonListener {
 public:
  explicit OmniboxTabSwitchButton(OmniboxResultView* result_view);

  // views::View
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener
  void ButtonPressed(Button* sender, const ui::Event& event) override {}

  // views::Button
  void StateChanged(ButtonState old_state) override;

 private:
  SkColor GetBackgroundColor() const;

  OmniboxResultView* result_view_;

  void SetPressed();

  DISALLOW_COPY_AND_ASSIGN(OmniboxTabSwitchButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_TAB_SWITCH_BUTTON_H_
