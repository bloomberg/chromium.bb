// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_CLIENT_H_

#include "base/metrics/field_trial.h"
#include "components/autofill/core/common/password_form.h"

class PasswordFormManager;
class PasswordManagerDriver;
class PrefService;
class Profile;

// An abstraction of operations in the external environment (WebContents)
// that the PasswordManager depends on.  This allows for more targeted
// unit testing.
class PasswordManagerClient {
 public:
  PasswordManagerClient() {}
  virtual ~PasswordManagerClient() {}

  // Informs the embedder of a password form that can be saved if the user
  // allows it. The embedder is not required to prompt the user if it decides
  // that this form doesn't need to be saved.
  virtual void PromptUserToSavePassword(PasswordFormManager* form_to_save) = 0;

  // Called when a password is autofilled. Default implementation is a no-op.
  virtual void PasswordWasAutofilled(
      const autofill::PasswordFormMap& best_matches) const {}

  // Get the profile for which we are managing passwords.
  // TODO(gcasto): Remove this function. crbug.com/335107.
  virtual Profile* GetProfile() = 0;

  // Gets prefs associated with this embedder.
  virtual PrefService* GetPrefs() = 0;

  // Returns the PasswordManagerDriver instance associated with this instance.
  virtual PasswordManagerDriver* GetDriver() = 0;

  // Returns the probability that the experiment identified by |experiment_name|
  // should be enabled. The default implementation returns 0.
  virtual base::FieldTrial::Probability GetProbabilityForExperiment(
      const std::string& experiment_name);

  // Returns true if password sync is enabled in the embedder. The default
  // implementation returns false.
  virtual bool IsPasswordSyncEnabled();

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerClient);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_CLIENT_H_
