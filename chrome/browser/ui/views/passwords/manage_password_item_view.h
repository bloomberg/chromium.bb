// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_H_

#include "chrome/browser/ui/views/location_bar/manage_passwords_bubble_view.h"
#include "components/autofill/core/common/password_form.h"
#include "ui/base/resource/resource_bundle.h"

namespace views {
class LabelButton;
}

// A custom view for credentials which allows the management of the specific
// credentials.
class ManagePasswordItemView : public views::View,
                               public views::ButtonListener {
 public:
  ManagePasswordItemView(
      ManagePasswordsBubbleModel* manage_passwords_bubble_model,
      autofill::PasswordForm password_form,
      int field_1_width,
      int field_2_width);

  static string16 GetPasswordDisplayString(const string16& password);

 private:
  virtual ~ManagePasswordItemView();

  // Changes the views according to the state of |delete_password_|.
  void Refresh();

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  views::Label* label_1_;
  views::Label* label_2_;

  // This button is used to set |delete_password_| and to change the view
  // accordingly. Clicking on it again will toggle it and restore the original
  // view.
  views::LabelButton* delete_or_undo_button_;

  ManagePasswordsBubbleModel* manage_passwords_bubble_model_;
  autofill::PasswordForm password_form_;
  bool delete_password_;
  int field_1_width_;
  int field_2_width_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_H_
