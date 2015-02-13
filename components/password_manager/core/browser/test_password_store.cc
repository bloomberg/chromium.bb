// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/test_password_store.h"

#include "components/autofill/core/common/password_form.h"

namespace password_manager {

TestPasswordStore::TestPasswordStore()
    : PasswordStore(base::MessageLoopProxy::current(),
                    base::MessageLoopProxy::current()) {
}

TestPasswordStore::~TestPasswordStore() {
}

const TestPasswordStore::PasswordMap& TestPasswordStore::stored_passwords()
    const {
  return stored_passwords_;
}

void TestPasswordStore::Clear() {
  stored_passwords_.clear();
}

bool TestPasswordStore::IsEmpty() const {
  // The store is empty, if the sum of all stored passwords across all entries
  // in |stored_passwords_| is 0.
  size_t number_of_passwords = 0u;
  for (PasswordMap::const_iterator it = stored_passwords_.begin();
       !number_of_passwords && it != stored_passwords_.end(); ++it) {
    number_of_passwords += it->second.size();
  }
  return number_of_passwords == 0u;
}

bool TestPasswordStore::FormsAreEquivalent(const autofill::PasswordForm& lhs,
                                           const autofill::PasswordForm& rhs) {
  return lhs.origin == rhs.origin &&
      lhs.username_element == rhs.username_element &&
      lhs.username_value == rhs.username_value &&
      lhs.password_element == rhs.password_element &&
      lhs.signon_realm == rhs.signon_realm;
}

void TestPasswordStore::GetAutofillableLoginsImpl(
    scoped_ptr<GetLoginsRequest> request) {
  ScopedVector<autofill::PasswordForm> results;
  for (const auto& forms_for_realm : stored_passwords_) {
    for (const autofill::PasswordForm& form : forms_for_realm.second)
      results.push_back(new autofill::PasswordForm(form));
  }
  request->NotifyConsumerWithResults(results.Pass());
}

PasswordStoreChangeList TestPasswordStore::AddLoginImpl(
    const autofill::PasswordForm& form) {
  PasswordStoreChangeList changes;
  stored_passwords_[form.signon_realm].push_back(form);
  changes.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  return changes;
}

PasswordStoreChangeList TestPasswordStore::UpdateLoginImpl(
    const autofill::PasswordForm& form) {
  PasswordStoreChangeList changes;
  std::vector<autofill::PasswordForm>& forms =
      stored_passwords_[form.signon_realm];
  for (std::vector<autofill::PasswordForm>::iterator it = forms.begin();
       it != forms.end(); ++it) {
    if (FormsAreEquivalent(form, *it)) {
      *it = form;
      changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form));
    }
  }
  return changes;
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginImpl(
    const autofill::PasswordForm& form) {
  PasswordStoreChangeList changes;
  std::vector<autofill::PasswordForm>& forms =
      stored_passwords_[form.signon_realm];
  std::vector<autofill::PasswordForm>::iterator it = forms.begin();
  while (it != forms.end()) {
    if (FormsAreEquivalent(form, *it)) {
      it = forms.erase(it);
      changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    } else {
      ++it;
    }
  }
  return changes;
}

ScopedVector<autofill::PasswordForm> TestPasswordStore::FillMatchingLogins(
    const autofill::PasswordForm& form,
    PasswordStore::AuthorizationPromptPolicy prompt_policy) {
  ScopedVector<autofill::PasswordForm> matched_forms;
  std::vector<autofill::PasswordForm> forms =
      stored_passwords_[form.signon_realm];
  for (const auto& stored_form : forms) {
    matched_forms.push_back(new autofill::PasswordForm(stored_form));
  }
  return matched_forms.Pass();
}

void TestPasswordStore::ReportMetricsImpl(const std::string& sync_username,
                                          bool custom_passphrase_sync_enabled) {
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginsCreatedBetweenImpl(
    base::Time begin,
    base::Time end) {
  PasswordStoreChangeList changes;
  return changes;
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginsSyncedBetweenImpl(
    base::Time begin,
    base::Time end) {
  PasswordStoreChangeList changes;
  return changes;
}

void TestPasswordStore::GetBlacklistLoginsImpl(
    scoped_ptr<GetLoginsRequest> request) {
}

bool TestPasswordStore::FillAutofillableLogins(
    ScopedVector<autofill::PasswordForm>* forms) {
  return true;
}

bool TestPasswordStore::FillBlacklistLogins(
    ScopedVector<autofill::PasswordForm>* forms) {
  return true;
}

}  // namespace password_manager
