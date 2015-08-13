// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/passwords_counter.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/common/pref_names.h"
#include "components/password_manager/core/browser/password_store.h"

PasswordsCounter::PasswordsCounter()
    : pref_name_(prefs::kDeletePasswords),
      store_(nullptr) {
}

PasswordsCounter::~PasswordsCounter() {
  if (store_)
    store_->RemoveObserver(this);
}

void PasswordsCounter::OnInitialized() {
  store_ = PasswordStoreFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS).get();
  if (store_)
    store_->AddObserver(this);
  else
    LOG(ERROR) << "No password store! Cannot count passwords.";
}

const std::string& PasswordsCounter::GetPrefName() const {
  return pref_name_;
}

void PasswordsCounter::Count() {
  if (!store_) {
    ReportResult(0);
    return;
  }

  cancelable_task_tracker()->TryCancelAll();
  // TODO(msramek): We don't actually need the logins themselves, just their
  // count. Consider implementing |PasswordStore::CountAutofillableLogins|.
  store_->GetAutofillableLogins(this);
}

void PasswordsCounter::OnGetPasswordStoreResults(
    ScopedVector<autofill::PasswordForm> results) {
  ReportResult(results.size());
}

void PasswordsCounter::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  RestartCounting();
}
