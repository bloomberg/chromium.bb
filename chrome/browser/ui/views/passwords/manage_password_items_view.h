// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEMS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEMS_VIEW_H_

#include <vector>

#include "base/memory/scoped_vector.h"
#include "components/autofill/core/common/password_form.h"
#include "ui/views/view.h"

class ManagePasswordsBubbleModel;

// A custom view of individual credentials. The view is represented as a table
// where each row can be in three distinct states:
//
// * Present credentials to the user which she may choose to save.
// * Present already-saved credentials to the user for management.
// * Offer the user the ability to undo a deletion action.
class ManagePasswordItemsView : public views::View {
 public:
  ManagePasswordItemsView(
      ManagePasswordsBubbleModel* manage_passwords_bubble_model,
      const std::vector<const autofill::PasswordForm*>& password_forms);

 private:
  class PasswordFormRow;

  ~ManagePasswordItemsView() override;

  void AddRows();
  void NotifyPasswordFormStatusChanged(
      const autofill::PasswordForm& password_form, bool deleted);

  // Changes the views according to the state of |password_forms_rows_|.
  void Refresh();

  ScopedVector<PasswordFormRow> password_forms_rows_;
  ManagePasswordsBubbleModel* model_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordItemsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEMS_VIEW_H_
