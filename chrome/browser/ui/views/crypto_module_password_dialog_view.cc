// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crypto_module_password_dialog_view.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

int kInputPasswordMinWidth = 8;

namespace browser {
// CryptoModulePasswordDialogView
////////////////////////////////////////////////////////////////////////////////
CryptoModulePasswordDialogView::CryptoModulePasswordDialogView(
    const std::string& slot_name,
    browser::CryptoModulePasswordReason reason,
    const std::string& server,
    const base::Callback<void(const char*)>& callback)
    : callback_(callback) {
  Init(server, slot_name, reason);
}

CryptoModulePasswordDialogView::~CryptoModulePasswordDialogView() {
}

void CryptoModulePasswordDialogView::Init(
    const std::string& server,
    const std::string& slot_name,
    browser::CryptoModulePasswordReason reason) {
  // Select an appropriate text for the reason.
  std::string text;
  const string16& server16 = UTF8ToUTF16(server);
  const string16& slot16 = UTF8ToUTF16(slot_name);
  switch (reason) {
    case browser::kCryptoModulePasswordKeygen:
      text = l10n_util::GetStringFUTF8(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_TEXT_KEYGEN, slot16, server16);
      break;
    case browser::kCryptoModulePasswordCertEnrollment:
      text = l10n_util::GetStringFUTF8(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_TEXT_CERT_ENROLLMENT, slot16, server16);
      break;
    case browser::kCryptoModulePasswordClientAuth:
      text = l10n_util::GetStringFUTF8(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_TEXT_CLIENT_AUTH, slot16, server16);
      break;
    case browser::kCryptoModulePasswordListCerts:
      text = l10n_util::GetStringFUTF8(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_TEXT_LIST_CERTS, slot16);
      break;
    case browser::kCryptoModulePasswordCertImport:
      text = l10n_util::GetStringFUTF8(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_TEXT_CERT_IMPORT, slot16);
      break;
    case browser::kCryptoModulePasswordCertExport:
      text = l10n_util::GetStringFUTF8(
          IDS_CRYPTO_MODULE_AUTH_DIALOG_TEXT_CERT_EXPORT, slot16);
      break;
    default:
      NOTREACHED();
  }
  reason_label_ = new views::Label(UTF8ToUTF16(text));
  reason_label_->SetMultiLine(true);

  password_label_ = new views::Label(l10n_util::GetStringUTF16(
      IDS_CRYPTO_MODULE_AUTH_DIALOG_PASSWORD_FIELD));

  password_entry_ = new views::Textfield(views::Textfield::STYLE_PASSWORD);
  password_entry_->SetController(this);

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  views::ColumnSet* reason_column_set = layout->AddColumnSet(0);
  reason_column_set->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::LEADING, 1,
      views::GridLayout::USE_PREF, 0, 0);

  views::ColumnSet* column_set = layout->AddColumnSet(1);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(
      0, views::kUnrelatedControlLargeHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(reason_label_);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, 1);
  layout->AddView(password_label_);
  layout->AddView(password_entry_);
}

views::View* CryptoModulePasswordDialogView::GetInitiallyFocusedView() {
  return password_entry_;
}
bool CryptoModulePasswordDialogView::IsModal() const {
  return true;
}
views::View* CryptoModulePasswordDialogView::GetContentsView() {
  return this;
}

string16 CryptoModulePasswordDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return UTF8ToUTF16(l10n_util::GetStringUTF8(
        IDS_CRYPTO_MODULE_AUTH_DIALOG_OK_BUTTON_LABEL));
  else if (button == ui::DIALOG_BUTTON_CANCEL)
    return UTF8ToUTF16(l10n_util::GetStringUTF8(IDS_CANCEL));
  const string16 empty;
  return empty;
}

bool CryptoModulePasswordDialogView::Accept() {
  callback_.Run(UTF16ToUTF8(password_entry_->text()).c_str());
  const string16 empty;
  password_entry_->SetText(empty);
  return true;
}

bool CryptoModulePasswordDialogView::Cancel() {
  callback_.Run(static_cast<const char*>(NULL));
  const string16 empty;
  password_entry_->SetText(empty);
  return true;
}

bool CryptoModulePasswordDialogView::HandleKeyEvent(
    views::Textfield* sender,
    const views::KeyEvent& keystroke) {
  return false;
}

void CryptoModulePasswordDialogView::ContentsChanged(
    views::Textfield* sender,
    const string16& new_contents) {
}

string16 CryptoModulePasswordDialogView::GetWindowTitle() const {
  return UTF8ToUTF16(l10n_util::GetStringUTF8(
      IDS_CRYPTO_MODULE_AUTH_DIALOG_TITLE));
}
}  // namespace browser
