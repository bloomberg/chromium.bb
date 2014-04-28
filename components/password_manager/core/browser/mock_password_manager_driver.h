// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_MANAGER_DRIVER_H_

#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordManagerDriver : public PasswordManagerDriver {
 public:
  MockPasswordManagerDriver();
  virtual ~MockPasswordManagerDriver();

  MOCK_METHOD1(FillPasswordForm, void(const autofill::PasswordFormFillData&));
  MOCK_METHOD0(DidLastPageLoadEncounterSSLErrors, bool());
  MOCK_METHOD0(IsOffTheRecord, bool());
  MOCK_METHOD0(GetPasswordGenerationManager, PasswordGenerationManager*());
  MOCK_METHOD0(GetPasswordManager, PasswordManager*());
  MOCK_METHOD0(GetAutofillManager, autofill::AutofillManager*());
  MOCK_METHOD0(GetPasswordAutofillManager, PasswordAutofillManager*());
  MOCK_METHOD1(AllowPasswordGenerationForForm, void(autofill::PasswordForm*));
  MOCK_METHOD1(AccountCreationFormsFound,
               void(const std::vector<autofill::FormData>&));
  MOCK_METHOD2(AcceptPasswordAutofillSuggestion,
               void(const base::string16&, const base::string16&));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_MANAGER_DRIVER_H_
