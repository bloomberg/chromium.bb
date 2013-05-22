// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/test_password_store.h"

#include "content/public/common/password_form.h"

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

bool TestPasswordStore::FormsAreEquivalent(const content::PasswordForm& lhs,
                                           const content::PasswordForm& rhs) {
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

void TestPasswordStore::AddLoginImpl(const content::PasswordForm& form) {
  stored_passwords_[form.signon_realm].push_back(form);
}

void TestPasswordStore::UpdateLoginImpl(const content::PasswordForm& form) {
  std::vector<content::PasswordForm>& forms =
      stored_passwords_[form.signon_realm];
  for (std::vector<content::PasswordForm>::iterator it = forms.begin();
         it != forms.end(); ++it) {
    if (FormsAreEquivalent(form, *it)) {
      *it = form;
    }
  }
}

void TestPasswordStore::RemoveLoginImpl(const content::PasswordForm& form) {
  std::vector<content::PasswordForm>& forms =
      stored_passwords_[form.signon_realm];
  for (std::vector<content::PasswordForm>::iterator it = forms.begin();
       it != forms.end(); ++it) {
    if (FormsAreEquivalent(form, *it)) {
      forms.erase(it);
      return;
    }
  }
}

void TestPasswordStore::GetLoginsImpl(
    const content::PasswordForm& form,
    const PasswordStore::ConsumerCallbackRunner& runner) {
  std::vector<content::PasswordForm*> matched_forms;
  std::vector<content::PasswordForm> forms =
      stored_passwords_[form.signon_realm];
  for (std::vector<content::PasswordForm>::iterator it = forms.begin();
       it != forms.end(); ++it) {
    matched_forms.push_back(new content::PasswordForm(*it));
  }
  runner.Run(matched_forms);
}

bool TestPasswordStore::FillAutofillableLogins(
    std::vector<content::PasswordForm*>* forms) {
  return true;
}

bool TestPasswordStore::FillBlacklistLogins(
    std::vector<content::PasswordForm*>* forms) {
  return true;
}
