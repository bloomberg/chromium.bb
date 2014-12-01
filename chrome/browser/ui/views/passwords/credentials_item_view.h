// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_

#include "base/macros.h"
#include "components/autofill/core/common/password_form.h"
#include "ui/views/controls/button/label_button.h"

namespace views {
class ImageView;
class Label;
}

// CredentialsItemView represents a credential view in the account chooser
// bubble.
class CredentialsItemView : public views::LabelButton {
 public:
  CredentialsItemView(views::ButtonListener* button_listener,
                      const autofill::PasswordForm& form);
  ~CredentialsItemView() override;

  const autofill::PasswordForm& form() const { return form_; }

 private:
  // views::LabelButton:
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int w) const override;
  void Layout() override;

  autofill::PasswordForm form_;

  views::ImageView* image_view_;
  views::Label* full_name_label_;
  views::Label* username_label_;

  DISALLOW_COPY_AND_ASSIGN(CredentialsItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_CREDENTIALS_ITEM_VIEW_H_
