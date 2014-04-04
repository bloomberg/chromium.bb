// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_password_item_view.h"

#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

ManagePasswordItemView::ManagePasswordItemView(
    ManagePasswordsBubbleModel* manage_passwords_bubble_model,
    autofill::PasswordForm password_form,
    int field_1_width,
    int field_2_width,
    Position position)
    : manage_passwords_bubble_model_(manage_passwords_bubble_model),
      password_form_(password_form),
      delete_password_(false),
      field_1_width_(field_1_width),
      field_2_width_(field_2_width) {
  views::GridLayout* layout = new views::GridLayout(this);
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  SetLayoutManager(layout);

  // When a password is displayed as the first item in a list, it has borders
  // on both the top and bottom. When it's in the middle of a list, or at the
  // end, it has a border only on the bottom.
  SetBorder(views::Border::CreateSolidSidedBorder(
      position == FIRST_ITEM ? 1 : 0,
      0,
      1,
      0,
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor)));

  // Build the columnset we need for the current state of the model.
  int column_set_to_build =
      !manage_passwords_bubble_model_->WaitingToSavePassword()
          ? COLUMN_SET_MANAGE
          : COLUMN_SET_SAVE;
  BuildColumnSet(layout, column_set_to_build);
  layout->StartRowWithPadding(
      0, column_set_to_build, 0, views::kRelatedControlVerticalSpacing);

  // Add the username field: fills the first non-padding column of the layout.
  label_1_ = new views::Label(password_form_.username_value);
  label_1_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(label_1_);

  // Add the password field: fills the second non-padding column of the layout.
  label_2_ =
      new views::Link(password_form_.password_value);
  label_2_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_2_->set_listener(this);
  label_2_->SetObscured(true);
  label_2_->SetFocusable(false);
  label_2_->SetEnabled(false);
  label_2_->SetUnderline(false);
  layout->AddView(label_2_);

  // If we're managing passwords (that is, we're not currently in the process
  // of saving a password), construct and add the delete button: fills the
  // third non-padding column of the layout.
  if (!manage_passwords_bubble_model_->WaitingToSavePassword()) {
    delete_button_ = new views::ImageButton(this);
    delete_button_->SetImage(views::ImageButton::STATE_NORMAL,
                             rb->GetImageNamed(IDR_CLOSE_2).ToImageSkia());
    delete_button_->SetImage(views::ImageButton::STATE_HOVERED,
                             rb->GetImageNamed(IDR_CLOSE_2_H).ToImageSkia());
    delete_button_->SetImage(views::ImageButton::STATE_PRESSED,
                             rb->GetImageNamed(IDR_CLOSE_2_P).ToImageSkia());
    layout->AddView(delete_button_, 1, 1);
  }

  // Match the padding at the top of the row with padding at the bottom.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
}

void ManagePasswordItemView::BuildColumnSet(views::GridLayout* layout,
                                            int column_set_id) {
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);

  // The username field.
  column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        0,
                        views::GridLayout::FIXED,
                        field_1_width_,
                        field_1_width_);

  // The password field.
  column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        field_2_width_,
                        field_2_width_);

  // If we're in manage-mode, we need another column for the delete button.
  if (column_set_id == COLUMN_SET_MANAGE) {
    column_set->AddColumn(views::GridLayout::TRAILING,
                          views::GridLayout::FILL,
                          0,
                          views::GridLayout::USE_PREF,
                          0,
                          0);
  }
  column_set->AddPaddingColumn(0, views::kItemLabelSpacing);
}

ManagePasswordItemView::~ManagePasswordItemView() {
  if (delete_password_)
    manage_passwords_bubble_model_->DeleteFromBestMatches(password_form_);
}

void ManagePasswordItemView::Refresh() {
  // TODO(mkwst): We're currently swaping out values in the same view. We need
  // to swap out views in order to enable some future work (and to make the undo
  // button's alignment work correctly).

  if (delete_password_) {
    // The user clicked the "delete password" button, so:
    //
    // Change the username string to "Deleted"
    label_1_->SetText(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED));

    // Change the password's text to "Undo", and enable the link.
    label_2_->SetText(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO));
    label_2_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    label_2_->SetObscured(false);
    label_2_->SetEnabled(true);
    label_2_->SetFocusable(true);

    if (delete_button_)
      delete_button_->SetVisible(false);
  } else {
    // The user clicked the "undo" button after deleting a password, so:

    // Move focus to the parent in order to get the focus ring off the link
    // that the user just clicked.
    parent()->RequestFocus();

    // Change the username string back to the username.
    label_1_->SetText(password_form_.username_value);

    // Change the second column back to the password, obscure and disable it.
    // disable the link.
    label_2_->SetObscured(true);
    label_2_->SetEnabled(false);
    label_2_->SetFocusable(false);
    label_2_->SetText(password_form_.password_value);
    label_2_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    if (delete_button_)
      delete_button_->SetVisible(true);
  }

  // After the view is consistent, notify the model that the password needs to
  // be updated (either removed or put back into the store, as appropriate.
  manage_passwords_bubble_model_->OnPasswordAction(
      password_form_,
      delete_password_ ? ManagePasswordsBubbleModel::REMOVE_PASSWORD
                       : ManagePasswordsBubbleModel::ADD_PASSWORD);
}

void ManagePasswordItemView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  DCHECK_EQ(delete_button_, sender);
  delete_password_ = true;
  Refresh();
}

void ManagePasswordItemView::LinkClicked(views::Link* source,
                                         int event_flags) {
  DCHECK_EQ(source, label_2_);
  delete_password_ = false;
  Refresh();
}
