// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "url/gurl.h"

namespace password_manager {
struct CredentialInfo;
class PasswordFormManager;
class PasswordManagerClient;
}


// ManagePasswordsState keeps the current state for ManagePasswordsUIController
// as well as up-to-date data for this state.
class ManagePasswordsState {
 public:
  using CredentialsCallback =
      base::Callback<void(const password_manager::CredentialInfo&)>;

  ManagePasswordsState();
  ~ManagePasswordsState();

  // Set the client for logging.
  void set_client(password_manager::PasswordManagerClient* client) {
    client_ = client;
  }

  // The methods below discard the current state/data of the object and move it
  // to the specified state.

  // Move to PENDING_PASSWORD_STATE.
  void OnPendingPassword(
      scoped_ptr<password_manager::PasswordFormManager> form_manager);

  // Move to CREDENTIAL_REQUEST_STATE.
  void OnRequestCredentials(
      ScopedVector<autofill::PasswordForm> local_credentials,
      ScopedVector<autofill::PasswordForm> federated_credentials,
      const GURL& origin);

  // Move to AUTO_SIGNIN_STATE. |local_forms| can't be empty.
  void OnAutoSignin(ScopedVector<autofill::PasswordForm> local_forms);

  // Move to CONFIRMATION_STATE.
  void OnAutomaticPasswordSave(
      scoped_ptr<password_manager::PasswordFormManager> form_manager);

  // Move to MANAGE_STATE or INACTIVE_STATE for PSL matched passwords.
  void OnPasswordAutofilled(const autofill::PasswordFormMap& password_form_map);

  // Move to BLACKLIST_STATE.
  void OnBlacklistBlockedAutofill(
      const autofill::PasswordFormMap& password_form_map);

  // Move to INACTIVE_STATE.
  void OnInactive();

  // Moves the object to |state| without resetting the internal data. Allowed:
  // * -> BLACKLIST_STATE
  // * -> MANAGE_STATE
  void TransitionToState(password_manager::ui::State state);

  // Updates the internal state applying |changes|.
  void ProcessLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes);

  password_manager::ui::State state() const { return state_; }
  const GURL& origin() const { return origin_; }
  password_manager::PasswordFormManager* form_manager() const {
    return form_manager_.get();
  }
  const CredentialsCallback& credentials_callback() {
    return credentials_callback_;
  }
  void set_credentials_callback(const CredentialsCallback& callback) {
    credentials_callback_ = callback;
  }

  // Current local forms. ManagePasswordsState is responsible for the forms.
  const std::vector<const autofill::PasswordForm*>& GetCurrentForms() const {
    return form_manager_ ? current_forms_weak_ : local_credentials_forms_.get();
  }

  // Current federated forms.
  const std::vector<const autofill::PasswordForm*>&
  federated_credentials_forms() const {
    return federated_credentials_forms_.get();
  }

 private:
  // Removes all the PasswordForms stored in this object.
  void ClearData();

  // Add |form| to the internal state.
  void AddForm(const autofill::PasswordForm& form);
  // Updates |form| in the internal state.
  bool UpdateForm(const autofill::PasswordForm& form);
  // Removes |form| from the internal state.
  void DeleteForm(const autofill::PasswordForm& form);

  void SetState(password_manager::ui::State state);

  // The origin of the current page. It's used to determine which PasswordStore
  // changes are applicable to the internal state.
  GURL origin_;

  // Contains the password that was submitted.
  scoped_ptr<password_manager::PasswordFormManager> form_manager_;

  // Weak references to the passwords for the current status. The hard pointers
  // are scattered between |form_manager_| and |local_credentials_forms_|. If
  // |form_manager_| is nullptr then all the forms are stored in
  // |local_credentials_forms_|. |current_forms_weak_| remains empty.
  std::vector<const autofill::PasswordForm*> current_forms_weak_;

  // If |form_manager_| is nullptr then |local_credentials_forms_| contains all
  // the current forms. Otherwise, it's a container for the new forms coming
  // from the PasswordStore.
  ScopedVector<const autofill::PasswordForm> local_credentials_forms_;

  // Federated credentials for the CREDENTIAL_REQUEST_STATE.
  ScopedVector<const autofill::PasswordForm> federated_credentials_forms_;

  // A callback to be invoked when user selects a credential.
  CredentialsCallback credentials_callback_;

  // The current state of the password manager UI.
  password_manager::ui::State state_;

  // The client used for logging.
  password_manager::PasswordManagerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsState);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_STATE_H_
