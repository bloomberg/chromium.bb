// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/login_view.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

static const int kMessageWidth = 320;
static const int kTextfieldStackHorizontalSpacing = 30;

using password_manager::LoginModel;
using views::GridLayout;

///////////////////////////////////////////////////////////////////////////////
// LoginView, public:

LoginView::LoginView(const base::string16& explanation,
                     LoginModel* model)
    : username_field_(new views::Textfield()),
      password_field_(new views::Textfield()),
      username_label_(new views::Label(
          l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_USERNAME_FIELD))),
      password_label_(new views::Label(
          l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_PASSWORD_FIELD))),
      message_label_(new views::Label(explanation)),
      login_model_(model) {
  password_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_label_->SetAllowCharacterBreak(true);

  // Initialize the Grid Layout Manager used for this dialog box.
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // Add the column set for the information message at the top of the dialog
  // box.
  const int single_column_view_set_id = 0;
  views::ColumnSet* column_set =
      layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::FIXED, kMessageWidth, 0);

  // Add the column set for the user name and password fields and labels.
  const int labels_column_set_id = 1;
  column_set = layout->AddColumnSet(labels_column_set_id);
  column_set->AddPaddingColumn(0, kTextfieldStackHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kTextfieldStackHorizontalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(message_label_);

  layout->AddPaddingRow(0, views::kUnrelatedControlLargeVerticalSpacing);

  layout->StartRow(0, labels_column_set_id);
  layout->AddView(username_label_);
  layout->AddView(username_field_);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, labels_column_set_id);
  layout->AddView(password_label_);
  layout->AddView(password_field_);

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  if (login_model_)
    login_model_->AddObserver(this);
}

LoginView::~LoginView() {
  if (login_model_)
    login_model_->RemoveObserver(this);
}

const base::string16& LoginView::GetUsername() const {
  return username_field_->text();
}

const base::string16& LoginView::GetPassword() const {
  return password_field_->text();
}

views::View* LoginView::GetInitiallyFocusedView() {
  return username_field_;
}

///////////////////////////////////////////////////////////////////////////////
// LoginView, views::View, password_manager::LoginModelObserver overrides:

void LoginView::OnAutofillDataAvailable(const base::string16& username,
                                        const base::string16& password) {
  if (username_field_->text().empty()) {
    username_field_->SetText(username);
    password_field_->SetText(password);
    username_field_->SelectAll(true);
  }
}

void LoginView::OnLoginModelDestroying() {
  login_model_->RemoveObserver(this);
  login_model_ = NULL;
}
