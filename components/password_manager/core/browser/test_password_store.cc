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

TestPasswordStore::~TestPasswordStore() {}

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
       !number_of_passwords && it != stored_passwords_.end();
       ++it) {
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

void TestPasswordStore::WrapModificationTask(ModificationTask task) {
  task.Run();
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
      changes.push_back(
          PasswordStoreChange(PasswordStoreChange::UPDATE, form));
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
      changes.push_back(
          PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    } else {
      ++it;
    }
  }
  return changes;
}

void TestPasswordStore::GetLoginsImpl(
    const autofill::PasswordForm& form,
    PasswordStore::AuthorizationPromptPolicy prompt_policy,
    const PasswordStore::ConsumerCallbackRunner& runner) {
  std::vector<autofill::PasswordForm*> matched_forms;
  std::vector<autofill::PasswordForm> forms =
      stored_passwords_[form.signon_realm];
  for (std::vector<autofill::PasswordForm>::iterator it = forms.begin();
       it != forms.end(); ++it) {
    matched_forms.push_back(new autofill::PasswordForm(*it));
  }
  runner.Run(matched_forms);
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

bool TestPasswordStore::FillAutofillableLogins(
    std::vector<autofill::PasswordForm*>* forms) {
  return true;
}

bool TestPasswordStore::FillBlacklistLogins(
    std::vector<autofill::PasswordForm*>* forms) {
  return true;
}

}  // namespace password_manager
