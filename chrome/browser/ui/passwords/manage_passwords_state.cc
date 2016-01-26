// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_state.h"

#include <utility>

#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/credential_manager_types.h"

using password_manager::PasswordFormManager;
using autofill::PasswordFormMap;

namespace {

// Returns a vector containing the values of a map.
template <typename Map>
std::vector<typename Map::mapped_type> MapToVector(const Map& map) {
  std::vector<typename Map::mapped_type> ret;
  ret.reserve(map.size());
  for (const auto& form_pair : map)
    ret.push_back(form_pair.second);
  return ret;
}

// Takes a ScopedPtrMap. Returns a vector of non-owned pointers to the elements
// inside the scoped_ptrs.
template <typename Map>
std::vector<const typename Map::mapped_type::element_type*>
ScopedPtrMapToVector(const Map& map) {
  std::vector<const typename Map::mapped_type::element_type*> ret;
  ret.reserve(map.size());
  for (const auto& form_pair : map)
    ret.push_back(form_pair.second.get());
  return ret;
}

ScopedVector<const autofill::PasswordForm> DeepCopyMapToVector(
    const PasswordFormMap& password_form_map) {
  ScopedVector<const autofill::PasswordForm> ret;
  ret.reserve(password_form_map.size());
  for (const auto& form_pair : password_form_map)
    ret.push_back(new autofill::PasswordForm(*form_pair.second));
  return ret;
}

ScopedVector<const autofill::PasswordForm> ConstifyVector(
    ScopedVector<autofill::PasswordForm>* forms) {
  ScopedVector<const autofill::PasswordForm> ret;
  ret.assign(forms->begin(), forms->end());
  forms->weak_clear();
  return ret;
}

// Updates one form in |forms| that has the same unique key as |updated_form|.
// Returns true if the form was found and updated.
bool UpdateFormInVector(const autofill::PasswordForm& updated_form,
                        ScopedVector<const autofill::PasswordForm>* forms) {
  ScopedVector<const autofill::PasswordForm>::iterator it = std::find_if(
      forms->begin(), forms->end(),
      [&updated_form](const autofill::PasswordForm* form) {
    return ArePasswordFormUniqueKeyEqual(*form, updated_form);
  });
  if (it != forms->end()) {
    delete *it;
    *it = new autofill::PasswordForm(updated_form);
    return true;
  }
  return false;
}

// Removes a form from |forms| that has the same unique key as |form_to_delete|.
template <class Vector>
void RemoveFormFromVector(const autofill::PasswordForm& form_to_delete,
                          Vector* forms) {
  typename Vector::iterator it = std::find_if(
      forms->begin(), forms->end(),
      [&form_to_delete](const autofill::PasswordForm* form) {
    return ArePasswordFormUniqueKeyEqual(*form, form_to_delete);
  });
  if (it != forms->end())
    forms->erase(it);
}

}  // namespace

ManagePasswordsState::ManagePasswordsState()
    : state_(password_manager::ui::INACTIVE_STATE),
      client_(nullptr) {
}

ManagePasswordsState::~ManagePasswordsState() {}

void ManagePasswordsState::OnPendingPassword(
      scoped_ptr<password_manager::PasswordFormManager> form_manager) {
  ClearData();
  form_manager_ = std::move(form_manager);
  current_forms_weak_ = ScopedPtrMapToVector(form_manager_->best_matches());
  origin_ = form_manager_->pending_credentials().origin;
  SetState(password_manager::ui::PENDING_PASSWORD_STATE);
}

void ManagePasswordsState::OnUpdatePassword(
    scoped_ptr<password_manager::PasswordFormManager> form_manager) {
  ClearData();
  form_manager_ = std::move(form_manager);
  current_forms_weak_ = ScopedPtrMapToVector(form_manager_->best_matches());
  origin_ = form_manager_->pending_credentials().origin;
  SetState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

void ManagePasswordsState::OnRequestCredentials(
    ScopedVector<autofill::PasswordForm> local_credentials,
    ScopedVector<autofill::PasswordForm> federated_credentials,
    const GURL& origin) {
  ClearData();
  local_credentials_forms_ = ConstifyVector(&local_credentials);
  federated_credentials_forms_ = ConstifyVector(&federated_credentials);
  origin_ = origin;
  SetState(password_manager::ui::CREDENTIAL_REQUEST_STATE);
}

void ManagePasswordsState::OnAutoSignin(
    ScopedVector<autofill::PasswordForm> local_forms) {
  DCHECK(!local_forms.empty());
  ClearData();
  local_credentials_forms_ = ConstifyVector(&local_forms);
  origin_ = local_credentials_forms_[0]->origin;
  SetState(password_manager::ui::AUTO_SIGNIN_STATE);
}

void ManagePasswordsState::OnAutomaticPasswordSave(
    scoped_ptr<PasswordFormManager> form_manager) {
  ClearData();
  form_manager_ = std::move(form_manager);
  autofill::ConstPasswordFormMap current_forms;
  for (const auto& match : form_manager_->best_matches()) {
    current_forms.insert(std::make_pair(match.first, match.second.get()));
  }
  current_forms[form_manager_->pending_credentials().username_value] =
      &form_manager_->pending_credentials();
  current_forms_weak_ = MapToVector(current_forms);
  origin_ = form_manager_->pending_credentials().origin;
  SetState(password_manager::ui::CONFIRMATION_STATE);
}

void ManagePasswordsState::OnPasswordAutofilled(
    const PasswordFormMap& password_form_map,
    const GURL& origin) {
  DCHECK(!password_form_map.empty());
  ClearData();
  bool only_PSL_matches =
      find_if(password_form_map.begin(), password_form_map.end(),
              [](const std::pair<const base::string16,
                                 scoped_ptr<autofill::PasswordForm>>& p) {
                return !p.second->is_public_suffix_match;
              }) == password_form_map.end();
  if (only_PSL_matches) {
    // Don't show the UI for PSL matched passwords. They are not stored for this
    // page and cannot be deleted.
    origin_ = GURL();
    SetState(password_manager::ui::INACTIVE_STATE);
  } else {
    local_credentials_forms_ = DeepCopyMapToVector(password_form_map);
    origin_ = origin;
    SetState(password_manager::ui::MANAGE_STATE);
  }
}

void ManagePasswordsState::OnInactive() {
  ClearData();
  origin_ = GURL();
  SetState(password_manager::ui::INACTIVE_STATE);
}

void ManagePasswordsState::TransitionToState(
    password_manager::ui::State state) {
  DCHECK_NE(password_manager::ui::INACTIVE_STATE, state_);
  DCHECK_EQ(password_manager::ui::MANAGE_STATE, state);
  if (state_ == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    if (!credentials_callback_.is_null()) {
      credentials_callback_.Run(password_manager::CredentialInfo());
      credentials_callback_.Reset();
    }
    federated_credentials_forms_.clear();
  }
  SetState(state);
}

void ManagePasswordsState::ProcessLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  if (state() == password_manager::ui::INACTIVE_STATE)
    return;

  for (const password_manager::PasswordStoreChange& change : changes) {
    const autofill::PasswordForm& changed_form = change.form();
    if (changed_form.blacklisted_by_user)
      continue;
    if (change.type() == password_manager::PasswordStoreChange::REMOVE) {
      DeleteForm(changed_form);
    } else {
      if (change.type() == password_manager::PasswordStoreChange::UPDATE)
        UpdateForm(changed_form);
      else
        AddForm(changed_form);
    }
  }
}

void ManagePasswordsState::ChooseCredential(
    const autofill::PasswordForm& form,
    password_manager::CredentialType credential_type) {
  DCHECK_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE, state());
  DCHECK(!credentials_callback().is_null());

  // Here, |credential_type| refers to whether the credential was originally
  // passed into ::OnRequestCredentials as part of the |local_credentials| or
  // |federated_credentials| lists (e.g. whether it is an existing credential
  // saved for this origin, or whether we should synthesize a new
  // FederatedCredential).
  //
  // If |credential_type| is federated, the credential MUST be returned as
  // a FederatedCredential in order to prevent password information leaking
  // cross-origin.
  //
  // If |credential_type| is local, the credential MIGHT be a PasswordCredential
  // or it MIGHT be a FederatedCredential. We inspect the |federation_url|
  // field to determine which we should return.
  //
  // TODO(mkwst): Clean this up. It is confusing.
  password_manager::CredentialType type_to_return;
  if (credential_type ==
          password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD &&
      form.federation_url.is_empty()) {
    type_to_return = password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD;
  } else if (credential_type ==
             password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY) {
    type_to_return = password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY;
  } else {
    type_to_return =
        password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED;
  }
  password_manager::CredentialInfo info =
      password_manager::CredentialInfo(form, type_to_return);
  credentials_callback().Run(info);
  set_credentials_callback(ManagePasswordsState::CredentialsCallback());
}

void ManagePasswordsState::ClearData() {
  form_manager_.reset();
  current_forms_weak_.clear();
  local_credentials_forms_.clear();
  federated_credentials_forms_.clear();
  credentials_callback_.Reset();
}

void ManagePasswordsState::AddForm(const autofill::PasswordForm& form) {
  if (form.origin != origin_)
    return;
  if (UpdateForm(form))
    return;
  if (form_manager_) {
    local_credentials_forms_.push_back(new autofill::PasswordForm(form));
    current_forms_weak_.push_back(local_credentials_forms_.back());
  } else {
    local_credentials_forms_.push_back(new autofill::PasswordForm(form));
  }
}

bool ManagePasswordsState::UpdateForm(const autofill::PasswordForm& form) {
  if (form_manager_) {
    // |current_forms_weak_| contains the list of current passwords.
    std::vector<const autofill::PasswordForm*>::iterator it = std::find_if(
        current_forms_weak_.begin(), current_forms_weak_.end(),
        [&form](const autofill::PasswordForm* current_form) {
      return ArePasswordFormUniqueKeyEqual(form, *current_form);
    });
    if (it != current_forms_weak_.end()) {
      RemoveFormFromVector(form, &local_credentials_forms_);
      local_credentials_forms_.push_back(new autofill::PasswordForm(form));
      *it = local_credentials_forms_.back();
      return true;
    }
  } else {
    // |current_forms_weak_| isn't used.
    bool updated_locals = UpdateFormInVector(form, &local_credentials_forms_);
    return (UpdateFormInVector(form, &federated_credentials_forms_) ||
            updated_locals);
  }
  return false;
}

void ManagePasswordsState::DeleteForm(const autofill::PasswordForm& form) {
  RemoveFormFromVector(form, &current_forms_weak_);
  RemoveFormFromVector(form, &local_credentials_forms_);
  RemoveFormFromVector(form, &federated_credentials_forms_);
}

void ManagePasswordsState::SetState(password_manager::ui::State state) {
  DCHECK(client_);
  if (client_->GetLogManager()->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        client_->GetLogManager());
    logger.LogNumber(
        autofill::SavePasswordProgressLogger::STRING_NEW_UI_STATE,
        state);
  }
  state_ = state;
}
