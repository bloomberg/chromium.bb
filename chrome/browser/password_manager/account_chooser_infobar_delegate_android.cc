// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/account_chooser_infobar_delegate_android.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/account_chooser_infobar.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/common/credential_manager_types.h"

// static
void AccountChooserInfoBarDelegateAndroid::Create(
    InfoBarService* infobar_service,
    ManagePasswordsUIController* ui_controller) {
  infobar_service->AddInfoBar(
      make_scoped_ptr(new AccountChooserInfoBar(make_scoped_ptr(
          new AccountChooserInfoBarDelegateAndroid(ui_controller)))));
}

AccountChooserInfoBarDelegateAndroid::AccountChooserInfoBarDelegateAndroid(
    ManagePasswordsUIController* ui_controller)
    : ui_controller_(ui_controller) {
}

AccountChooserInfoBarDelegateAndroid::~AccountChooserInfoBarDelegateAndroid() {
}

void AccountChooserInfoBarDelegateAndroid::ChooseCredential(
    size_t credential_index,
    password_manager::CredentialType credential_type) {
  using namespace password_manager;
  if (credential_type == CredentialType::CREDENTIAL_TYPE_EMPTY) {
    ui_controller_->ChooseCredential(autofill::PasswordForm(), credential_type);
    return;
  }
  DCHECK(credential_type == CredentialType::CREDENTIAL_TYPE_LOCAL ||
         credential_type == CredentialType::CREDENTIAL_TYPE_FEDERATED);
  auto& credentials_forms =
      (credential_type == CredentialType::CREDENTIAL_TYPE_LOCAL)
          ? ui_controller_->local_credentials_forms()
          : ui_controller_->federated_credentials_forms();
  if (credential_index < credentials_forms.size()) {
    ui_controller_->ChooseCredential(*credentials_forms[credential_index],
                                     credential_type);
  }
}

void AccountChooserInfoBarDelegateAndroid::InfoBarDismissed() {
  ChooseCredential(-1, password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY);
}

infobars::InfoBarDelegate::Type
AccountChooserInfoBarDelegateAndroid::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}
