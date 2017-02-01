// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/login_view.h"

#include "chrome/browser/ui/views/harmony/layout_delegate.h"
#include "chrome/browser/ui/views/layout_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"

static const int kMessageWidth = 320;
static const int kTextfieldStackHorizontalSpacing = 30;

using password_manager::LoginModel;
using views::GridLayout;

///////////////////////////////////////////////////////////////////////////////
// LoginView, public:

LoginView::LoginView(const base::string16& authority,
                     const base::string16& explanation,
                     LoginHandler::LoginModelData* login_model_data)
    : username_field_(new views::Textfield()),
      password_field_(new views::Textfield()),
      username_label_(new views::Label(
          l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_USERNAME_FIELD))),
      password_label_(new views::Label(
          l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_PASSWORD_FIELD))),
      authority_label_(new views::Label(authority)),
      message_label_(nullptr),
      login_model_(login_model_data ? login_model_data->model : nullptr) {
  LayoutDelegate* layout_delegate = LayoutDelegate::Get();
  password_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  authority_label_->SetMultiLine(true);
  authority_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  authority_label_->SetAllowCharacterBreak(true);

  // Initialize the Grid Layout Manager used for this dialog box.
  GridLayout* layout = layout_utils::CreatePanelLayout(this);

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
  if (layout_delegate->UseExtraDialogPadding())
    column_set->AddPaddingColumn(0, kTextfieldStackHorizontalSpacing);
  column_set->AddColumn(layout_delegate->GetControlLabelGridAlignment(),
                        GridLayout::CENTER, 0, GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(
      0,
      layout_delegate->GetMetric(
          LayoutDelegate::Metric::RELATED_CONTROL_HORIZONTAL_SPACING));
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  if (layout_delegate->UseExtraDialogPadding())
    column_set->AddPaddingColumn(0, kTextfieldStackHorizontalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(authority_label_);
  if (!explanation.empty()) {
    message_label_ = new views::Label(explanation);
    message_label_->SetMultiLine(true);
    message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message_label_->SetAllowCharacterBreak(true);
    layout->AddPaddingRow(
        0,
        layout_delegate->GetMetric(
            LayoutDelegate::Metric::RELATED_CONTROL_VERTICAL_SPACING));
    layout->StartRow(0, single_column_view_set_id);
    layout->AddView(message_label_);
  }

  layout->AddPaddingRow(
      0,
      layout_delegate->GetMetric(
          LayoutDelegate::Metric::UNRELATED_CONTROL_VERTICAL_SPACING_LARGE));

  layout->StartRow(0, labels_column_set_id);
  layout->AddView(username_label_);
  layout->AddView(username_field_);

  layout->AddPaddingRow(
      0,
      layout_delegate->GetMetric(
          LayoutDelegate::Metric::RELATED_CONTROL_VERTICAL_SPACING));

  layout->StartRow(0, labels_column_set_id);
  layout->AddView(password_label_);
  layout->AddView(password_field_);

  if (layout_delegate->UseExtraDialogPadding()) {
    layout->AddPaddingRow(
        0,
        layout_delegate->GetMetric(
            LayoutDelegate::Metric::UNRELATED_CONTROL_VERTICAL_SPACING));
  }

  if (login_model_data) {
    login_model_->AddObserverAndDeliverCredentials(this,
                                                   login_model_data->form);
  }
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

void LoginView::OnAutofillDataAvailableInternal(
    const base::string16& username,
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

const char* LoginView::GetClassName() const {
  return "LoginView";
}

