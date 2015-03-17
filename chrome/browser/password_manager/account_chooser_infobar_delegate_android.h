// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_INFOBAR_DELEGATE_ANDROID_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "components/infobars/core/infobar_delegate.h"

class InfoBarService;

namespace password_manager {
enum class CredentialType : unsigned int;
}

namespace autofill {
struct PasswordForm;
}

// Android-only infobar delegate to allow user to choose credentials for login.
class AccountChooserInfoBarDelegateAndroid : public infobars::InfoBarDelegate {
 public:
  // Creates an account chooser infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     ManagePasswordsUIController* ui_controller);

  ~AccountChooserInfoBarDelegateAndroid() override;

  const std::vector<const autofill::PasswordForm*>&
  local_credentials_forms() const {
    return ui_controller_->GetCurrentForms();
  }

  void ChooseCredential(size_t credential_index,
                        password_manager::CredentialType credential_type);

 private:
  explicit AccountChooserInfoBarDelegateAndroid(
      ManagePasswordsUIController* ui_controller);

  // infobars::InfoBarDelegate:
  void InfoBarDismissed() override;
  Type GetInfoBarType() const override;

  // Owned by WebContents.
  ManagePasswordsUIController* ui_controller_;

  DISALLOW_COPY_AND_ASSIGN(AccountChooserInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_INFOBAR_DELEGATE_ANDROID_H_
