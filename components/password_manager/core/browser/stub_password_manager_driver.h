// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_DRIVER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_manager_driver.h"

namespace password_manager {

// Use this class as a base for mock or test drivers to avoid stubbing
// uninteresting pure virtual methods. All the implemented methods are just
// trivial stubs. Do NOT use in production, only use in tests.
class StubPasswordManagerDriver : public PasswordManagerDriver {
 public:
  StubPasswordManagerDriver();
  ~StubPasswordManagerDriver() override;

  // PasswordManagerDriver:
  int GetId() const override;
  void FillPasswordForm(
      const autofill::PasswordFormFillData& form_data) override;
  void GeneratedPasswordAccepted(const base::string16& password) override;
  void FillSuggestion(const base::string16& username,
                      const base::string16& password) override;
  void PreviewSuggestion(const base::string16& username,
                         const base::string16& password) override;
  void ClearPreviewedForm() override;
  PasswordGenerationFrameHelper* GetPasswordGenerationHelper() override;
  PasswordManager* GetPasswordManager() override;
  PasswordAutofillManager* GetPasswordAutofillManager() override;
  autofill::AutofillDriver* GetAutofillDriver() override;
  bool IsMainFrame() const override;
  bool CanShowAutofillUi() const override;
  const GURL& GetLastCommittedURL() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StubPasswordManagerDriver);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_STUB_PASSWORD_MANAGER_DRIVER_H_
