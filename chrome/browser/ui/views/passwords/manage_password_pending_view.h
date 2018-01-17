// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_PENDING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_PENDING_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ToggleImageButton;
class Combobox;
class Label;
}  // namespace views

class ManagePasswordsBubbleView;

// A view offering the user the ability to save credentials. Contains a
// username and password field, along with a "Save Passwords" button and a
// "Never" button.
class ManagePasswordPendingView : public views::View,
                                  public views::ButtonListener {
 public:
  explicit ManagePasswordPendingView(ManagePasswordsBubbleView* parent);
  ~ManagePasswordPendingView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // View:
  gfx::Size CalculatePreferredSize() const override;

  void CreateAndSetLayout(bool show_password_label);
  void CreatePasswordField();
  void TogglePasswordVisibility();
  void UpdateUsernameAndPasswordInModel();
  void ReplaceWithPromo();

  ManagePasswordsBubbleView* parent_;

  views::Button* save_button_;
  views::Button* never_button_;
  views::View* username_field_;
  views::ToggleImageButton* password_view_button_;

  // The view for the password value. Only one of |password_dropdown_| and
  // |password_label_| should be available.
  views::Combobox* password_dropdown_;
  views::Label* password_label_;

  bool are_passwords_revealed_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordPendingView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_PENDING_VIEW_H_
