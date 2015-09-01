// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_INFOBAR_DELEGATE_ANDROID_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/gfx/range/range.h"

namespace password_manager {
enum class CredentialType;
}

namespace autofill {
struct PasswordForm;
}

namespace content {
class WebContents;
}

// Android-only infobar delegate to allow user to choose credentials for login.
class AccountChooserInfoBarDelegateAndroid : public infobars::InfoBarDelegate {
 public:
  // Creates an account chooser infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(content::WebContents* web_contents,
                     ManagePasswordsUIController* ui_controller);

  ~AccountChooserInfoBarDelegateAndroid() override;

  const std::vector<const autofill::PasswordForm*>&
  local_credentials_forms() const {
    return ui_controller_->GetCurrentForms();
  }

  const std::vector<const autofill::PasswordForm*>&
  federated_credentials_forms() const {
    return ui_controller_->GetFederatedForms();
  }

  void ChooseCredential(size_t credential_index,
                        password_manager::CredentialType credential_type);

  // Returns the translated text of the message to display.
  const base::string16& message() const { return message_; }

  // Returns the range of the message text that should be a link.
  const gfx::Range& message_link_range() const { return message_link_range_; }

  void LinkClicked();

 private:
  explicit AccountChooserInfoBarDelegateAndroid(
      content::WebContents* web_contents,
      ManagePasswordsUIController* ui_controller);

  // infobars::InfoBarDelegate:
  void InfoBarDismissed() override;
  Type GetInfoBarType() const override;

  // Owned by WebContents.
  ManagePasswordsUIController* ui_controller_;

  // Title for the infobar: branded as a part of Google Smart Lock for signed
  // users.
  base::string16 message_;

  // If set, describes the location of the link.
  gfx::Range message_link_range_;

  DISALLOW_COPY_AND_ASSIGN(AccountChooserInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_INFOBAR_DELEGATE_ANDROID_H_
