// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_ui.h"

namespace autofill {
struct PasswordForm;
}
namespace content {
class WebContents;
}
namespace password_manager {
struct InteractionsStats;
}
class GURL;

// An interface for ManagePasswordsBubbleModel implemented by
// ManagePasswordsUIController. Allows to retrieve the current state of the tab
// and notify about user actions.
class PasswordsModelDelegate {
 public:
  // Returns WebContents* the model is attached to.
  virtual content::WebContents* GetWebContents() const = 0;

  // Returns the URL of the site the current forms are retrieved for.
  virtual const GURL& GetOrigin() const = 0;

  // Returns the current tab state.
  virtual password_manager::ui::State GetState() const = 0;

  // Returns the pending password in PENDING_PASSWORD_STATE and
  // PENDING_PASSWORD_UPDATE_STATE, the saved password in CONFIRMATION_STATE,
  // the returned credential in AUTO_SIGNIN_STATE.
  virtual const autofill::PasswordForm& GetPendingPassword() const = 0;

  // True if the password for previously stored account was overridden, i.e. in
  // newly submitted form the password is different from stored one.
  virtual bool IsPasswordOverridden() const = 0;

  // Returns current local forms for the current page.
  virtual const std::vector<std::unique_ptr<autofill::PasswordForm>>&
  GetCurrentForms() const = 0;

  // For PENDING_PASSWORD_STATE state returns the current statistics for
  // the pending username.
  virtual const password_manager::InteractionsStats*
  GetCurrentInteractionStats() const = 0;

  // Called from the model when the bubble is displayed.
  virtual void OnBubbleShown() = 0;

  // Called from the model when the bubble is hidden.
  virtual void OnBubbleHidden() = 0;

  // Called when the user didn't interact with UI.
  virtual void OnNoInteraction() = 0;

  // Called when the user chose not to update password.
  virtual void OnNopeUpdateClicked() = 0;

  // Called from the model when the user chooses to never save passwords.
  virtual void NeverSavePassword() = 0;

  // Called from the model when the user chooses to save a password.
  virtual void SavePassword() = 0;

  // Called from the model when the user chooses to update a password.
  virtual void UpdatePassword(const autofill::PasswordForm& password_form) = 0;

  // Called from the dialog controller when the user chooses a credential.
  // Controller can be destroyed inside the method.
  virtual void ChooseCredential(
      const autofill::PasswordForm& form,
      password_manager::CredentialType credential_type) = 0;

  // Open a new tab, pointing to the Smart Lock help article.
  virtual void NavigateToSmartLockHelpPage() = 0;
  // Open a new tab, pointing to passwords.google.com.
  virtual void NavigateToPasswordManagerAccountDashboard() = 0;
  // Open a new tab, pointing to the password manager settings page.
  virtual void NavigateToPasswordManagerSettingsPage() = 0;
  // Starts the Chrome Sign in flow.
  virtual void NavigateToChromeSignIn() = 0;

  // Called from the dialog controller when the dialog is hidden.
  virtual void OnDialogHidden() = 0;

 protected:
  virtual ~PasswordsModelDelegate() = default;
};

base::WeakPtr<PasswordsModelDelegate>
PasswordsModelDelegateFromWebContents(content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_H_
