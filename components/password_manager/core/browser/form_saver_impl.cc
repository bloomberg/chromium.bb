// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_saver_impl.h"

#include "base/auto_reset.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_store.h"

using autofill::PasswordForm;

namespace password_manager {

FormSaverImpl::FormSaverImpl(PasswordStore* store) : store_(store) {
  DCHECK(store);
}

FormSaverImpl::~FormSaverImpl() = default;

void FormSaverImpl::PermanentlyBlacklist(PasswordForm* observed) {
  observed->preferred = false;
  observed->blacklisted_by_user = true;
  observed->other_possible_usernames.clear();
  observed->date_created = base::Time::Now();

  store_->AddLogin(*observed);
}

void FormSaverImpl::Save(const PasswordForm& pending,
                         const autofill::PasswordFormMap& best_matches,
                         const PasswordForm* old_primary_key) {
  SaveImpl(pending, true, best_matches, nullptr, old_primary_key);
}

void FormSaverImpl::Update(
    const PasswordForm& pending,
    const autofill::PasswordFormMap& best_matches,
    const std::vector<const PasswordForm*>* credentials_to_update,
    const PasswordForm* old_primary_key) {
  SaveImpl(pending, false, best_matches, credentials_to_update,
           old_primary_key);
}

void FormSaverImpl::PresaveGeneratedPassword(const PasswordForm& generated) {
  if (presaved_)
    store_->UpdateLoginWithPrimaryKey(generated, *presaved_);
  else
    store_->AddLogin(generated);
  presaved_.reset(new PasswordForm(generated));
}

void FormSaverImpl::RemovePresavedPassword() {
  if (!presaved_)
    return;

  store_->RemoveLogin(*presaved_);
  presaved_ = nullptr;
}

void FormSaverImpl::SaveImpl(
    const PasswordForm& pending,
    bool is_new_login,
    const autofill::PasswordFormMap& best_matches,
    const std::vector<const PasswordForm*>* credentials_to_update,
    const PasswordForm* old_primary_key) {
  // Empty and null |credentials_to_update| mean the same, having a canonical
  // representation as nullptr makes the code simpler.
  if (credentials_to_update && credentials_to_update->empty())
    credentials_to_update = nullptr;

  DCHECK(pending.preferred);
  DCHECK(!pending.blacklisted_by_user);

  base::AutoReset<const autofill::PasswordFormMap*> ar1(&best_matches_,
                                                        &best_matches);
  base::AutoReset<const PasswordForm*> ar2(&pending_, &pending);

  UpdatePreferredLoginState();
  if (presaved_) {
    store_->UpdateLoginWithPrimaryKey(*pending_, *presaved_);
    presaved_ = nullptr;
  } else if (is_new_login) {
    store_->AddLogin(*pending_);
    if (!pending_->username_value.empty())
      DeleteEmptyUsernameCredentials();
  } else {
    if (old_primary_key)
      store_->UpdateLoginWithPrimaryKey(*pending_, *old_primary_key);
    else
      store_->UpdateLogin(*pending_);
  }

  if (credentials_to_update) {
    for (const PasswordForm* credential : *credentials_to_update) {
      store_->UpdateLogin(*credential);
    }
  }
}

void FormSaverImpl::UpdatePreferredLoginState() {
  const base::string16& preferred_username = pending_->username_value;
  for (const auto& key_value_pair : *best_matches_) {
    const auto& form = key_value_pair.second;
    if (form->preferred && !form->is_public_suffix_match &&
        form->username_value != preferred_username) {
      // This wasn't the selected login but it used to be preferred.
      form->preferred = false;
      store_->UpdateLogin(*form);
    }
  }
}

void FormSaverImpl::DeleteEmptyUsernameCredentials() {
  DCHECK(!pending_->username_value.empty());

  for (const auto& match : *best_matches_) {
    const PasswordForm* form = match.second.get();
    if (!form->is_public_suffix_match && form->username_value.empty() &&
        form->password_value == pending_->password_value) {
      store_->RemoveLogin(*form);
    }
  }
}

}  // namespace password_manager
