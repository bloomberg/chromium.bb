// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_

// This file was contains some repeated code from
// chrome/renderer/autofill/password_autofill_manager because as we move to the
// new Autofill UI we needs these functions in both the browser and renderer.
// Once the move is completed the repeated code in the renderer half should be
// removed.
// http://crbug.com/51644

#include <map>

#include "components/autofill/core/common/password_form_fill_data.h"

namespace autofill {

class AutofillDriver;

// This class is responsible for filling password forms.
class PasswordAutofillManager {
 public:
  explicit PasswordAutofillManager(AutofillDriver* autofill_driver);
  virtual ~PasswordAutofillManager();

  // Fills the password associated with user name |username|. Returns true if
  // the username and password fields were filled, false otherwise.
  bool DidAcceptAutofillSuggestion(const FormFieldData& field,
                                   const base::string16& username);

  // Invoked when a password mapping is added.
  void AddPasswordFormMapping(
      const FormFieldData& username_element,
      const PasswordFormFillData& password);

  // Invoked to clear any page specific cached values.
  void Reset();

 private:
  // TODO(csharp): Modify the AutofillExternalDeletegate code so that it can
  // figure out if a entry is a password one without using this mapping.
  // crbug.com/118601
  typedef std::map<FormFieldData,
                   PasswordFormFillData>
      LoginToPasswordInfoMap;

  // Returns true if |current_username| matches a username for one of the
  // login mappings in |password|.
  bool WillFillUserNameAndPassword(
      const base::string16& current_username,
      const PasswordFormFillData& password);

  // Finds login information for a |node| that was previously filled.
  bool FindLoginInfo(const FormFieldData& field,
                     PasswordFormFillData* found_password);

  // The logins we have filled so far with their associated info.
  LoginToPasswordInfoMap login_to_password_info_;

  // Provides driver-level context to the shared code of the component. Must
  // outlive |this|.
  AutofillDriver* const autofill_driver_;  // weak

  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_
