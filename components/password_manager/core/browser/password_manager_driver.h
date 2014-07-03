// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace autofill {
class AutofillManager;
struct FormData;
struct PasswordForm;
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {

class PasswordAutofillManager;
class PasswordGenerationManager;
class PasswordManager;

// Interface that allows PasswordManager core code to interact with its driver
// (i.e., obtain information from it and give information to it).
class PasswordManagerDriver {
 public:
  PasswordManagerDriver() {}
  virtual ~PasswordManagerDriver() {}

  // Fills forms matching |form_data|.
  virtual void FillPasswordForm(
      const autofill::PasswordFormFillData& form_data) = 0;

  // Returns whether any SSL certificate errors were encountered as a result of
  // the last page load.
  virtual bool DidLastPageLoadEncounterSSLErrors() = 0;

  // If this browsing session should not be persisted.
  virtual bool IsOffTheRecord() = 0;

  // Informs the driver that |form| can be used for password generation.
  virtual void AllowPasswordGenerationForForm(
      const autofill::PasswordForm& form) = 0;

  // Notifies the driver that account creation |forms| were found.
  virtual void AccountCreationFormsFound(
      const std::vector<autofill::FormData>& forms) = 0;

  // Tells the driver to fill the form with the |username| and |password|.
  virtual void FillSuggestion(const base::string16& username,
                              const base::string16& password) = 0;

  // Tells the driver to preview filling form with the |username| and
  // |password|.
  virtual void PreviewSuggestion(const base::string16& username,
                                 const base::string16& password) = 0;

  // Tells the driver to clear previewed password and username fields.
  virtual void ClearPreviewedForm() = 0;

  // Returns the PasswordGenerationManager associated with this instance.
  virtual PasswordGenerationManager* GetPasswordGenerationManager() = 0;

  // Returns the PasswordManager associated with this instance.
  virtual PasswordManager* GetPasswordManager() = 0;

  // Returns the PasswordAutofillManager associated with this instance.
  virtual PasswordAutofillManager* GetPasswordAutofillManager() = 0;

  // Returns the AutofillManager associated with this instance.
  virtual autofill::AutofillManager* GetAutofillManager() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerDriver);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_
