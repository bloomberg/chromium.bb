// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/passwords_counter.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/password_manager/core/browser/password_store.h"

PasswordsCounter::PasswordsCounter(Profile* profile)
    : BrowsingDataCounter(prefs::kDeletePasswords), profile_(profile) {}

PasswordsCounter::~PasswordsCounter() {
  store_->RemoveObserver(this);
}

void PasswordsCounter::OnInitialized() {
  store_ = PasswordStoreFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(store_);
  store_->AddObserver(this);
}

void PasswordsCounter::Count() {
  cancelable_task_tracker()->TryCancelAll();
  // TODO(msramek): We don't actually need the logins themselves, just their
  // count. Consider implementing |PasswordStore::CountAutofillableLogins|.
  // This custom request should also allow us to specify the time range, so that
  // we can use it to filter the login creation date in the database.
  store_->GetAutofillableLogins(this);
}

void PasswordsCounter::OnGetPasswordStoreResults(
    ScopedVector<autofill::PasswordForm> results) {
  base::Time start = GetPeriodStart();
  ReportResult(std::count_if(
      results.begin(),
      results.end(),
      [start](const autofill::PasswordForm* form) {
        return form->date_created >= start;
      }));
}

void PasswordsCounter::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  Restart();
}
