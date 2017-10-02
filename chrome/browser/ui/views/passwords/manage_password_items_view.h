// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEMS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEMS_VIEW_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/autofill/core/common/password_form.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
class Label;
}  // namespace views

class ManagePasswordsBubbleModel;

// Standalone functions for creating username and password views.
std::unique_ptr<views::Label> CreateUsernameLabel(
    const autofill::PasswordForm& form);
std::unique_ptr<views::Label> CreatePasswordLabel(
    const autofill::PasswordForm& form,
    bool is_password_visible);
std::unique_ptr<views::Textfield> CreateUsernameEditable(
    const autofill::PasswordForm& form);

// A custom view of individual credentials. The view is represented as a table
// where each row can offer the user the ability to undo a deletion action.
class ManagePasswordItemsView : public views::View {
 public:
  ManagePasswordItemsView(
      ManagePasswordsBubbleModel* manage_passwords_bubble_model,
      const std::vector<autofill::PasswordForm>* password_forms,
      int bubble_width);

 private:
  class PasswordFormRow;

  ~ManagePasswordItemsView() override;

  void AddRows();
  void NotifyPasswordFormStatusChanged(
      const autofill::PasswordForm& password_form, bool deleted);

  // Changes the views according to the state of |password_forms_rows_|.
  void Refresh();

  std::vector<std::unique_ptr<PasswordFormRow>> password_forms_rows_;
  ManagePasswordsBubbleModel* model_;
  const int bubble_width_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordItemsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEMS_VIEW_H_
