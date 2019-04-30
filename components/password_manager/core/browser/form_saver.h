// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {

// This interface allows the caller to save passwords and blacklist entries in
// a password store.
class FormSaver {
 public:
  FormSaver() = default;

  virtual ~FormSaver() = default;

  // Configures the |observed| form to become a blacklist entry and saves it.
  virtual void PermanentlyBlacklist(autofill::PasswordForm* observed) = 0;

  // Saves the |pending| form.
  // |matches| are relevant credentials for the site. After saving |pending|,
  // the following clean up steps are performed on the credentials stored on
  // disk that correspond to |matches|:
  // - the |preferred| state is reset to false.
  // - empty-username credentials with the same password are removed.
  // - if |old_password| is provided, the old credentials with the same username
  //   and the old password are updated to the new password.
  virtual void Save(const autofill::PasswordForm& pending,
                    const std::vector<const autofill::PasswordForm*>& matches,
                    const base::string16& old_password) = 0;

  // Updates the |pending| form and updates the stored preference on
  // |best_matches|. If |old_primary_key| is given, uses it for saving
  // |pending|. It also updates the password store with all
  // |credentials_to_update|.
  // TODO(crbug/831123): it's a convoluted interface, switch to the one for Save
  virtual void Update(
      const autofill::PasswordForm& pending,
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const std::vector<autofill::PasswordForm>* credentials_to_update,
      const autofill::PasswordForm* old_primary_key) = 0;

  // Removes |form| from the password store.
  virtual void Remove(const autofill::PasswordForm& form) = 0;

  // Creates a new FormSaver with the same state as |*this|.
  virtual std::unique_ptr<FormSaver> Clone() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FormSaver);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_SAVER_H_
