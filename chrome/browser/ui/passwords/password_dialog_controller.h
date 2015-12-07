// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_CONTROLLER_H_

#include <utility>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "ui/gfx/range/range.h"

namespace autofill {
struct PasswordForm;
}

// An interface used by the password dialog (the account chooser) for setting
// and retrieving the state.
class PasswordDialogController {
 public:
  using FormsVector = std::vector<scoped_ptr<autofill::PasswordForm>>;

  // Returns forms from the password database for the current site.
  virtual const FormsVector& GetLocalForms() const = 0;

  // Returns IDP's forms which can be used for logging in.
  virtual const FormsVector& GetFederationsForms() const = 0;

  // Returns a title of the account chooser and a range of the Smart Lock
  // hyperlink if it exists. If the range is empty then no hyperlink is shown.
  virtual std::pair<base::string16, gfx::Range> GetAccoutChooserTitle() const
      = 0;

  // Called when the Smart Lock hyperlink is clicked.
  virtual void OnSmartLockLinkClicked() = 0;

  // Called when the user chooses a credential.
  virtual void OnChooseCredentials(
      const autofill::PasswordForm& password_form,
      password_manager::CredentialType credential_type) = 0;
 protected:
  virtual ~PasswordDialogController() = default;
};


#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_DIALOG_CONTROLLER_H_
