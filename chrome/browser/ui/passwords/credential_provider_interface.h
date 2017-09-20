// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_PROVIDER_INTERFACE_H_
#define CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_PROVIDER_INTERFACE_H_

namespace autofill {
struct PasswordForm;
}

// Provides an interface for getting credentials, such as passwords, from some
// form of provider.
class CredentialProviderInterface {
 public:
  // Gets all password entries.
  virtual std::vector<std::unique_ptr<autofill::PasswordForm>>
  GetAllPasswords() = 0;

 protected:
  virtual ~CredentialProviderInterface() {}
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_CREDENTIAL_PROVIDER_INTERFACE_H_
