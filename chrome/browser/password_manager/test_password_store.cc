// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/test_password_store.h"

#include "components/autofill/core/common/password_form.h"

// static
scoped_refptr<RefcountedBrowserContextKeyedService> TestPasswordStore::Create(
    content::BrowserContext* profile) {
  return make_scoped_refptr(new TestPasswordStore);
}

TestPasswordStore::TestPasswordStore() : PasswordStore() {}
TestPasswordStore::~TestPasswordStore() {}

TestPasswordStore::PasswordMap TestPasswordStore::stored_passwords() {
  return stored_passwords_;
}

void TestPasswordStore::Clear() {
  stored_passwords_.clear();
}

bool TestPasswordStore::FormsAreEquivalent(const autofill::PasswordForm& lhs,
                                           const autofill::PasswordForm& rhs) {
  return lhs.origin == rhs.origin &&
      lhs.username_element == rhs.username_element &&
      lhs.username_value == rhs.username_value &&
      lhs.password_element == rhs.password_element &&
      lhs.signon_realm == rhs.signon_realm;
}

bool TestPasswordStore::ScheduleTask(const base::Closure& task) {
  task.Run();
  return true;
}

void TestPasswordStore::WrapModificationTask(base::Closure task) {
  task.Run();
}

void TestPasswordStore::AddLoginImpl(const autofill::PasswordForm& form) {
  stored_passwords_[form.signon_realm].push_back(form);
}

void TestPasswordStore::UpdateLoginImpl(const autofill::PasswordForm& form) {
  std::vector<autofill::PasswordForm>& forms =
      stored_passwords_[form.signon_realm];
  for (std::vector<autofill::PasswordForm>::iterator it = forms.begin();
         it != forms.end(); ++it) {
    if (FormsAreEquivalent(form, *it)) {
      *it = form;
    }
  }
}

void TestPasswordStore::RemoveLoginImpl(const autofill::PasswordForm& form) {
  std::vector<autofill::PasswordForm>& forms =
      stored_passwords_[form.signon_realm];
  for (std::vector<autofill::PasswordForm>::iterator it = forms.begin();
       it != forms.end(); ++it) {
    if (FormsAreEquivalent(form, *it)) {
      forms.erase(it);
      return;
    }
  }
}

void TestPasswordStore::GetLoginsImpl(
    const autofill::PasswordForm& form,
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

bool TestPasswordStore::FillAutofillableLogins(
    std::vector<autofill::PasswordForm*>* forms) {
  return true;
}

bool TestPasswordStore::FillBlacklistLogins(
    std::vector<autofill::PasswordForm*>* forms) {
  return true;
}
