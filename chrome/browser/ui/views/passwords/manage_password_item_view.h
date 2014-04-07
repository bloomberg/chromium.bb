// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_H_

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "components/autofill/core/common/password_form.h"

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

  enum Position { FIRST_ITEM, SUBSEQUENT_ITEM };

  ManagePasswordItemView(
      ManagePasswordsBubbleModel* manage_passwords_bubble_model,
      autofill::PasswordForm password_form,
      int field_1_width,
      int field_2_width,
      Position position);

 private:
  enum ColumnSets { TWO_COLUMN_SET = 0, THREE_COLUMN_SET };

  virtual ~ManagePasswordItemView();

  views::Label* GenerateUsernameLabel() const;
  views::Label* GeneratePasswordLabel() const;

  // Build a two-label column set using the widths stored in |field_1_width_|
  // and |field_2_width_|.
  void BuildColumnSet(views::GridLayout*, int column_set_id);

  void NotifyClickedDelete();
  void NotifyClickedUndo();

  // Changes the views according to the state of |delete_password_|.
  void Refresh();

  ManagePasswordsBubbleModel* manage_passwords_bubble_model_;
  autofill::PasswordForm password_form_;
  bool delete_password_;
  int field_1_width_;
  int field_2_width_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORD_ITEM_VIEW_H_
