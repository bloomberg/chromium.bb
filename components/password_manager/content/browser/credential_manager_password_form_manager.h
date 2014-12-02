// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CREDENTIAL_MANAGER_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CREDENTIAL_MANAGER_PASSWORD_FORM_MANAGER_H_

#include "components/password_manager/core/browser/password_form_manager.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

class ContentCredentialManagerDispatcher;
class PasswordManagerClient;
class PasswordManagerDriver;

// A PasswordFormManager built to handle PassworForm objects synthesized
// by the Credential Manager API.
class CredentialManagerPasswordFormManager : public PasswordFormManager {
 public:
  // Given a |client| and an |observed_form|, kick off the process of fetching
  // matching logins from the password store; if |observed_form| doesn't map to
  // a blacklisted origin, provisionally save it. Once saved, let the dispatcher
  // know that it's safe to poke at the UI.
  //
  // This class does not take ownership of |dispatcher|.
  CredentialManagerPasswordFormManager(
      PasswordManagerClient* client,
      PasswordManagerDriver* driver,
      const autofill::PasswordForm& observed_form,
      ContentCredentialManagerDispatcher* dispatcher);
  ~CredentialManagerPasswordFormManager() override;

  void OnGetPasswordStoreResults(
      const std::vector<autofill::PasswordForm*>& results) override;

 private:
  ContentCredentialManagerDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerPasswordFormManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_CREDENTIAL_MANAGER_PASSWORD_FORM_MANAGER_H_
