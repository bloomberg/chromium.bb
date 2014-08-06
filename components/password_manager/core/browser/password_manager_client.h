// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_

#include "base/metrics/field_trial.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_store.h"

class PrefService;

namespace password_manager {

class PasswordFormManager;
class PasswordManagerDriver;
class PasswordStore;

// An abstraction of operations that depend on the embedders (e.g. Chrome)
// environment.
class PasswordManagerClient {
 public:
  PasswordManagerClient() {}
  virtual ~PasswordManagerClient() {}

  // For automated testing, the save password prompt should sometimes not be
  // shown, and password immediately saved instead. That can be enforced by
  // a command-line flag. If auto-saving is enforced, this method returns true.
  // The default return value is false.
  virtual bool IsAutomaticPasswordSavingEnabled() const;

  // If the password manager should work for the current page. Default
  // always returns true.
  virtual bool IsPasswordManagerEnabledForCurrentPage() const;

  // Returns true if |username| and |origin| correspond to the account which is
  // syncing.
  virtual bool IsSyncAccountCredential(
      const std::string& username, const std::string& origin) const = 0;

  // Informs the embedder of a password form that can be saved if the user
  // allows it. The embedder is not required to prompt the user if it decides
  // that this form doesn't need to be saved.
  virtual void PromptUserToSavePassword(
      scoped_ptr<PasswordFormManager> form_to_save) = 0;

  // Called when a password is saved in an automated fashion. Embedder may
  // inform the user that this save has occured.
  virtual void AutomaticPasswordSave(
      scoped_ptr<PasswordFormManager> saved_form_manager) = 0;

  // Called when a password is autofilled. |best_matches| contains the
  // PasswordForm into which a password was filled: the client may choose to
  // save this to the PasswordStore, for example. Default implementation is a
  // noop.
  virtual void PasswordWasAutofilled(
      const autofill::PasswordFormMap& best_matches) const {}

  // Called when password autofill is blocked by the blacklist. |best_matches|
  // contains the PasswordForm that flags the current site as being on the
  // blacklist. The client may choose to remove this from the PasswordStore in
  // order to unblacklist a site, for example. Default implementation is a noop.
  virtual void PasswordAutofillWasBlocked(
      const autofill::PasswordFormMap& best_matches) const {}

  // Called to authenticate the autofill password data.  If authentication is
  // successful, this should continue filling the form.
  virtual void AuthenticateAutofillAndFillForm(
      scoped_ptr<autofill::PasswordFormFillData> fill_data) = 0;

  // Gets prefs associated with this embedder.
  virtual PrefService* GetPrefs() = 0;

  // Returns the PasswordStore associated with this instance.
  virtual PasswordStore* GetPasswordStore() = 0;

  // Returns the PasswordManagerDriver instance associated with this instance.
  virtual PasswordManagerDriver* GetDriver() = 0;

  // Returns the probability that the experiment identified by |experiment_name|
  // should be enabled. The default implementation returns 0.
  virtual base::FieldTrial::Probability GetProbabilityForExperiment(
      const std::string& experiment_name);

  // Returns true if password sync is enabled in the embedder. The default
  // implementation returns false.
  virtual bool IsPasswordSyncEnabled();

  // Only for clients which registered with a LogRouter: If called with
  // |router_can_be_used| set to false, the client may no longer use the
  // LogRouter. If |router_can_be_used| is true, the LogRouter can be used after
  // the return from OnLogRouterAvailabilityChanged.
  virtual void OnLogRouterAvailabilityChanged(bool router_can_be_used);

  // Forward |text| for display to the LogRouter (if registered with one).
  virtual void LogSavePasswordProgress(const std::string& text);

  // Returns true if logs recorded via LogSavePasswordProgress will be
  // displayed, and false otherwise.
  virtual bool IsLoggingActive() const;

  // Returns the authorization prompt policy to be used with the given form.
  // Only relevant on OSX.
  virtual PasswordStore::AuthorizationPromptPolicy GetAuthorizationPromptPolicy(
      const autofill::PasswordForm& form);

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerClient);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_
