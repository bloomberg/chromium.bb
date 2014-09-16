// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_H_

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "ui/views/view.h"

class ManagePasswordsBubbleModel;

// A custom view for credentials which allows the management of the specific
// credentials. The view has three distinct states:
//
// * Present credentials to the user which she may choose to save.
// * Present already-saved credentials to the user for management.
// * Offer the user the ability to undo a deletion action.
//
// The ManagePasswordItemView serves as a container for a single view
// representing one of these states.
class ManagePasswordItemView : public views::View {
 public:
  ManagePasswordItemView(
      ManagePasswordsBubbleModel* manage_passwords_bubble_model,
      const autofill::PasswordForm& password_form,
      password_manager::ui::PasswordItemPosition position);

 private:
  class ManageView;
  class PendingView;
  class UndoView;

  virtual ~ManagePasswordItemView();

  void NotifyClickedDelete();
  void NotifyClickedUndo();

  // Changes the views according to the state of |delete_password_|.
  void Refresh();

  ManagePasswordsBubbleModel* model_;
  autofill::PasswordForm password_form_;
  bool delete_password_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_H_
