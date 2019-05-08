// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_STATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_STATE_H_

#include <map>
#include <memory>

#include "base/optional.h"
#include "base/time/clock.h"
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

  const base::string16& generated_password() const {
    return presaved_->password_value;
  }

  // Called when generated password is accepted or changed by user.
  void PresaveGeneratedPassword(autofill::PasswordForm generated);

  // Signals that the user cancels password generation.
  void PasswordNoLongerGenerated();

  // Finish the generation flow by saving the final credential |generated|.
  // |matches| and |old_password| have the same meaning as in FormSaver.
  void CommitGeneratedPassword(
      autofill::PasswordForm generated,
      const std::vector<const autofill::PasswordForm*>& matches,
      const base::string16& old_password);

#if defined(UNIT_TEST)
  void set_clock(std::unique_ptr<base::Clock> clock) {
    clock_ = std::move(clock);
  }
#endif

 private:
  // Weak reference to the interface for saving credentials.
  FormSaver* const form_saver_;
  // Stores the pre-saved credential.
  base::Optional<autofill::PasswordForm> presaved_;
  // Interface to get current time.
  std::unique_ptr<base::Clock> clock_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_GENERATION_STATE_H_
