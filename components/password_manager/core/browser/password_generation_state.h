// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_STATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_STATE_H_

#include <map>
#include <memory>

#include "base/optional.h"
#include "components/autofill/core/common/password_form.h"

namespace password_manager {

class FormSaver;

class PasswordGenerationState {
 public:
  explicit PasswordGenerationState(FormSaver* form_saver);
  ~PasswordGenerationState();
  PasswordGenerationState(const PasswordGenerationState& rhs) = delete;
  PasswordGenerationState& operator=(const PasswordGenerationState&) = delete;

  std::unique_ptr<PasswordGenerationState> Clone(FormSaver* form_saver) const;

  // Returns true iff the generated password was presaved.
  bool HasGeneratedPassword() const { return presaved_.has_value(); }

  // Called when generated password is accepted or changed by user.
  void PresaveGeneratedPassword(autofill::PasswordForm generated);

  // Signals that the user cancels password generation.
  void PasswordNoLongerGenerated();

  // Finish the generation flow by saving the final credential |generated| and
  // leaving the generation state.
  // |best_matches| constains possible passwords for the current site. They will
  // be update according to the new preferred state.
  // |credentials_to_update| are credentials for probably related domain that
  // should be also updated.
  void CommitGeneratedPassword(
      const autofill::PasswordForm& generated,
      const std::map<base::string16, const autofill::PasswordForm*>&
          best_matches,
      const std::vector<autofill::PasswordForm>* credentials_to_update);

 private:
  // Weak reference to the interface for saving credentials.
  FormSaver* const form_saver_;
  // Stores the pre-saved credential.
  base::Optional<autofill::PasswordForm> presaved_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_STATE_H_
