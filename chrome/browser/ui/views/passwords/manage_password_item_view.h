// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_H_

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/password_manager_ui.h"

class ManagePasswordsBubbleModel;

namespace views {
class GridLayout;
class ImageButton;
}

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
  // Render credentials in two columns: username and password.
  class PendingView : public views::View {
   public:
    explicit PendingView(ManagePasswordItemView* parent);

   private:
    virtual ~PendingView();
  };

  // Render credentials in three columns: username, password, and delete.
  class ManageView : public views::View, public views::ButtonListener {
   public:
    explicit ManageView(ManagePasswordItemView* parent);

   private:
    virtual ~ManageView();

    // views::ButtonListener:
    virtual void ButtonPressed(views::Button* sender,
                               const ui::Event& event) OVERRIDE;

    views::ImageButton* delete_button_;
    ManagePasswordItemView* parent_;
  };

  // Render a notification to the user that a password has been removed, and
  // offer an undo link.
  class UndoView : public views::View, public views::LinkListener {
   public:
    explicit UndoView(ManagePasswordItemView* parent);

   private:
    virtual ~UndoView();

    // views::LinkListener:
    virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

    views::Link* undo_link_;
    ManagePasswordItemView* parent_;
  };

  ManagePasswordItemView(
      ManagePasswordsBubbleModel* manage_passwords_bubble_model,
      autofill::PasswordForm password_form,
      password_manager::ui::PasswordItemPosition position);

 private:
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
