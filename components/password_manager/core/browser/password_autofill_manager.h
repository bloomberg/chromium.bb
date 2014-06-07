// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_popup_delegate.h"
#include "components/autofill/core/common/password_form_fill_data.h"

namespace gfx {
class RectF;
}

namespace password_manager {

class PasswordManagerClient;

// This class is responsible for filling password forms.
class PasswordAutofillManager : public autofill::AutofillPopupDelegate {
 public:
  PasswordAutofillManager(PasswordManagerClient* password_manager_client,
                          autofill::AutofillClient* autofill_client);
  virtual ~PasswordAutofillManager();

  // AutofillPopupDelegate implementation.
  virtual void OnPopupShown() OVERRIDE;
  virtual void OnPopupHidden() OVERRIDE;
  virtual void DidSelectSuggestion(const base::string16& value,
                                   int identifier) OVERRIDE;
  virtual void DidAcceptSuggestion(const base::string16& value,
                                   int identifier) OVERRIDE;
  virtual void RemoveSuggestion(const base::string16& value,
                                int identifier) OVERRIDE;
  virtual void ClearPreviewedForm() OVERRIDE;

  // Invoked when a password mapping is added.
  void OnAddPasswordFormMapping(
      const autofill::FormFieldData& field,
      const autofill::PasswordFormFillData& fill_data);

  // Handles a request from the renderer to show a popup with the given
  // |suggestions| from the password manager.
  void OnShowPasswordSuggestions(
      const autofill::FormFieldData& field,
      const gfx::RectF& bounds,
      const std::vector<base::string16>& suggestions,
      const std::vector<base::string16>& realms);

  // Invoked to clear any page specific cached values.
  void Reset();

  // A public version of FillSuggestion(), only for use in tests.
  bool FillSuggestionForTest(const autofill::FormFieldData& field,
                             const base::string16& username);

  // A public version of PreviewSuggestion(), only for use in tests.
  bool PreviewSuggestionForTest(const autofill::FormFieldData& field,
                                const base::string16& username);

 private:
  typedef std::map<autofill::FormFieldData, autofill::PasswordFormFillData>
      LoginToPasswordInfoMap;

  // Attempts to fill the password associated with user name |username|, and
  // returns true if it was successful.
  bool FillSuggestion(const autofill::FormFieldData& field,
                      const base::string16& username);

  // Attempts to preview the password associated with user name |username|, and
  // returns true if it was successful.
  bool PreviewSuggestion(const autofill::FormFieldData& field,
                         const base::string16& username);

  // If |current_username| matches a username for one of the login mappings in
  // |fill_data|, returns true and assigns the password to |out_password|.
  // Otherwise, returns false and leaves |out_password| untouched.
  bool GetPasswordForUsername(
      const base::string16& current_username,
      const autofill::PasswordFormFillData& fill_data,
      base::string16* out_password);

  // Finds login information for a |node| that was previously filled.
  bool FindLoginInfo(const autofill::FormFieldData& field,
                     autofill::PasswordFormFillData* found_password);

  // The logins we have filled so far with their associated info.
  LoginToPasswordInfoMap login_to_password_info_;

  // Provides embedder-level operations on passwords. Must outlive |this|.
  PasswordManagerClient* const password_manager_client_;  // weak

  autofill::AutofillClient* const autofill_client_;  // weak

  // The form field on which the autofill popup is shown.
  autofill::FormFieldData form_field_;

  base::WeakPtrFactory<PasswordAutofillManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_
