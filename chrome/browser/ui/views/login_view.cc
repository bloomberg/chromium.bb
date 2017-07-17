// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/login_view.h"

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/harmony/textfield_layout.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"

using views::GridLayout;

namespace {

constexpr int kHeaderColumnSetId = 0;
constexpr int kFieldsColumnSetId = 1;
constexpr float kFixed = 0.f;
constexpr float kStretchy = 1.f;

// Adds a row to |layout| and puts a Label in it.
void AddHeaderLabel(GridLayout* layout,
                    const base::string16& text,
                    int text_style) {
  views::Label* label =
      new views::Label(text, views::style::CONTEXT_LABEL, text_style);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  layout->StartRow(kFixed, kHeaderColumnSetId);
  layout->AddView(label);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// LoginView, public:

LoginView::LoginView(const base::string16& authority,
                     const base::string16& explanation,
                     LoginHandler::LoginModelData* login_model_data)
    : login_model_(login_model_data ? login_model_data->model : nullptr) {
  // TODO(tapted): When Harmony is default, this should be removed and left up
  // to textfield_layout.h to decide.
  constexpr int kMessageWidth = 320;
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  // Initialize the Grid Layout Manager used for this dialog box.
  GridLayout* layout = GridLayout::CreatePanel(this);
  views::ColumnSet* column_set = layout->AddColumnSet(kHeaderColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, kStretchy,
                        GridLayout::FIXED, kMessageWidth, 0);
  AddHeaderLabel(layout, authority, views::style::STYLE_PRIMARY);
  AddHeaderLabel(layout, explanation, STYLE_SECONDARY);
  layout->AddPaddingRow(kFixed, provider->GetDistanceMetric(
                                    DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE));

  ConfigureTextfieldStack(layout, kFieldsColumnSetId);
  username_field_ = AddFirstTextfieldRow(
      layout, l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_USERNAME_FIELD),
      kFieldsColumnSetId);
  password_field_ = AddTextfieldRow(
      layout, l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_PASSWORD_FIELD),
      kFieldsColumnSetId);
  password_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  if (provider->UseExtraDialogPadding()) {
    layout->AddPaddingRow(kFixed,
                          provider->GetDistanceMetric(
                              views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
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

