// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/password_changed_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/textfield_with_margin.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/window/window.h"

using views::Button;
using views::GridLayout;
using views::Label;
using views::RadioButton;
using views::Textfield;

namespace chromeos {

namespace {
const int kPasswordFieldWidthChars = 20;
}  // namespace

PasswordChangedView::PasswordChangedView(Delegate* delegate,
                                         bool full_sync_disabled)
    : title_label_(NULL),
      description_label_(NULL),
      full_sync_radio_(NULL),
      delta_sync_radio_(NULL),
      old_password_field_(NULL),
      delegate_(delegate),
      full_sync_disabled_(full_sync_disabled) {
}

bool PasswordChangedView::Accept() {
  return ExitDialog();
}

int PasswordChangedView::GetDialogButtons() const {
 return MessageBoxFlags::DIALOGBUTTON_OK;
}

views::View* PasswordChangedView::GetInitiallyFocusedView() {
  if (!full_sync_disabled_) {
    return views::DialogDelegate::GetInitiallyFocusedView();
  } else {
    DCHECK(old_password_field_);
    return old_password_field_;
  }
}

std::wstring PasswordChangedView::GetWindowTitle() const {
  return UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_CHANGED_DIALOG_BOX_TITLE));
}

gfx::Size PasswordChangedView::GetPreferredSize() {
  // TODO(nkostylev): Once UI is finalized, create locale settings.
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_PASSWORD_CHANGED_DIALOG_WIDTH_CHARS,
      IDS_PASSWORD_CHANGED_DIALOG_HEIGHT_LINES));
}

void PasswordChangedView::ViewHierarchyChanged(bool is_add,
                                               views::View* parent,
                                               views::View* child) {
  if (is_add && child == this) {
    Init();
  }
}

void PasswordChangedView::Init() {
  // Set up fonts.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font = rb.GetFont(ResourceBundle::MediumBoldFont);

  // Create controls
  title_label_ = new Label();
  title_label_->SetFont(title_font);
  title_label_->SetText(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_CHANGED_TITLE)));
  title_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);

  description_label_ = new Label();
  description_label_->SetText(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_CHANGED_DESC)));
  description_label_->SetMultiLine(true);
  description_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);

  full_sync_radio_ = new RadioButton(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_CHANGED_RESET)),
      0);
  full_sync_radio_->set_listener(this);
  full_sync_radio_->SetMultiLine(true);

  delta_sync_radio_ = new RadioButton(
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_LOGIN_PASSWORD_CHANGED_MIGRATE)),
      0);
  delta_sync_radio_->set_listener(this);
  delta_sync_radio_->SetMultiLine(true);

  old_password_field_ = new TextfieldWithMargin(Textfield::STYLE_PASSWORD);
  old_password_field_->set_text_to_display_when_empty(
      l10n_util::GetStringUTF16(IDS_LOGIN_PREVIOUS_PASSWORD));
  old_password_field_->set_default_width_in_chars(kPasswordFieldWidthChars);
  old_password_field_->SetController(this);

  // Define controls layout.
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set = layout->AddColumnSet(1);
  column_set->AddPaddingColumn(
      0, views::kUnrelatedControlLargeHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(title_label_);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, 0);
  layout->AddView(description_label_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, 0);
  layout->AddView(full_sync_radio_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, 0);
  layout->AddView(delta_sync_radio_);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  layout->StartRow(0, 1);
  layout->AddView(
      old_password_field_, 1, 1, GridLayout::LEADING, GridLayout::CENTER);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, 0);
  layout->AddView(old_password_field_);

  // Disable options if needed.
  if (!full_sync_disabled_) {
    full_sync_radio_->SetChecked(true);
    old_password_field_->SetEnabled(false);
  } else {
    full_sync_radio_->SetEnabled(false);
    delta_sync_radio_->SetChecked(true);
    old_password_field_->SetEnabled(true);
  }

}

bool PasswordChangedView::ExitDialog() {
  if (delta_sync_radio_->checked() && old_password_field_->text().empty())
    return false;

  // TODO(nkostylev): Need to sanitize memory used to store password.
  if (full_sync_radio_->checked())
    delegate_->ResyncEncryptedData();
  else
    delegate_->RecoverEncryptedData(UTF16ToUTF8(old_password_field_->text()));

  return true;
}

void PasswordChangedView::ButtonPressed(Button* sender,
                                        const views::Event& event) {
  if (sender == full_sync_radio_) {
    old_password_field_->SetEnabled(false);
    old_password_field_->SetText(string16());
  } else if (sender == delta_sync_radio_) {
    old_password_field_->SetEnabled(true);
    old_password_field_->RequestFocus();
  }
}

}  // namespace chromeos
