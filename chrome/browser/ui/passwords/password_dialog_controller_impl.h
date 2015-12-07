// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_CONTROLLER_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"

class AccountChooserPrompt;
class Profile;

// A UI controller responsible for the account chooser dialog.
class PasswordDialogControllerImpl : public PasswordDialogController {
 public:
  explicit PasswordDialogControllerImpl(Profile* profle);
  ~PasswordDialogControllerImpl() override;

  // Pop up the account chooser dialog.
  void ShowAccountChooser(AccountChooserPrompt* dialog,
                          FormsVector locals, FormsVector federations);

  // PasswordDialogController:
  const FormsVector& GetLocalForms() const override;
  const FormsVector& GetFederationsForms() const override;
  std::pair<base::string16, gfx::Range> GetAccoutChooserTitle() const override;
  void OnSmartLockLinkClicked() override;
  void OnChooseCredentials(
      const autofill::PasswordForm& password_form,
      password_manager::CredentialType credential_type) override;

 private:
  // Release |current_dialog_| and close the open dialog.
  void ResetDialog();

  Profile* const profile_;
  AccountChooserPrompt* current_dialog_;
  std::vector<scoped_ptr<autofill::PasswordForm>> local_credentials_;
  std::vector<scoped_ptr<autofill::PasswordForm>> federated_credentials_;

  DISALLOW_COPY_AND_ASSIGN(PasswordDialogControllerImpl);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_CONTROLLER_IMPL_H_
