// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SAVE_CONFIRMATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SAVE_CONFIRMATION_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

class ManagePasswordsBubbleView;

// A view confirming to the user that a password was saved and offering a link
// to the Google account manager.
class ManagePasswordSaveConfirmationView : public views::View,
                                           public views::ButtonListener,
                                           public views::StyledLabelListener {
 public:
  explicit ManagePasswordSaveConfirmationView(
      ManagePasswordsBubbleView* parent);
  ~ManagePasswordSaveConfirmationView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener implementation
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  ManagePasswordsBubbleView* parent_;
  views::Button* ok_button_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordSaveConfirmationView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_SAVE_CONFIRMATION_VIEW_H_
