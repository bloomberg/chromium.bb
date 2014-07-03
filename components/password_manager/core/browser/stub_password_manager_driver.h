// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_DRIVER_H_

#include "components/password_manager/core/browser/password_manager_driver.h"

namespace password_manager {

// Use this class as a base for mock or test drivers to avoid stubbing
// uninteresting pure virtual methods. All the implemented methods are just
// trivial stubs. Do NOT use in production, only use in tests.
class StubPasswordManagerDriver : public PasswordManagerDriver {
 public:
  StubPasswordManagerDriver();
  virtual ~StubPasswordManagerDriver();

  // PasswordManagerDriver:
  virtual void FillPasswordForm(
      const autofill::PasswordFormFillData& form_data) OVERRIDE;
  virtual bool DidLastPageLoadEncounterSSLErrors() OVERRIDE;
  virtual bool IsOffTheRecord() OVERRIDE;
  virtual void AllowPasswordGenerationForForm(
      const autofill::PasswordForm& form) OVERRIDE;
  virtual void AccountCreationFormsFound(
      const std::vector<autofill::FormData>& forms) OVERRIDE;
  virtual void FillSuggestion(const base::string16& username,
                              const base::string16& password) OVERRIDE;
  virtual void PreviewSuggestion(const base::string16& username,
                                 const base::string16& password) OVERRIDE;
  virtual void ClearPreviewedForm() OVERRIDE;
  virtual PasswordGenerationManager* GetPasswordGenerationManager() OVERRIDE;
  virtual PasswordManager* GetPasswordManager() OVERRIDE;
  virtual PasswordAutofillManager* GetPasswordAutofillManager() OVERRIDE;
  virtual autofill::AutofillManager* GetAutofillManager() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(StubPasswordManagerDriver);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_DRIVER_H_
