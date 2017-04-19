// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/passwords_counter.h"

#include "components/browsing_data/core/pref_names.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/sync/driver/sync_service.h"

namespace {

bool IsPasswordSyncEnabled(const syncer::SyncService* sync_service) {
  if (!sync_service)
    return false;
  return password_manager_util::GetPasswordSyncState(sync_service) !=
         password_manager::PasswordSyncState::NOT_SYNCING_PASSWORDS;
}

}  // namespace

namespace browsing_data {

PasswordsCounter::PasswordsCounter(
    scoped_refptr<password_manager::PasswordStore> store,
    syncer::SyncService* sync_service)
    : store_(store), sync_service_(sync_service), password_sync_enabled_() {
  DCHECK(store_);
}

PasswordsCounter::~PasswordsCounter() {
  store_->RemoveObserver(this);
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

void PasswordsCounter::OnInitialized() {
  store_->AddObserver(this);
  if (sync_service_)
    sync_service_->AddObserver(this);
  password_sync_enabled_ = IsPasswordSyncEnabled(sync_service_);
}

const char* PasswordsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeletePasswords;
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
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  base::Time start = GetPeriodStart();
  int num_passwords = std::count_if(
      results.begin(), results.end(),
      [start](const std::unique_ptr<autofill::PasswordForm>& form) {
        return form->date_created >= start;
      });
  ReportResult(base::MakeUnique<PasswordResult>(this, num_passwords,
                                                password_sync_enabled_));
}

void PasswordsCounter::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  Restart();
}

void PasswordsCounter::OnStateChanged(syncer::SyncService* sync) {
  bool sync_enabled_new = IsPasswordSyncEnabled(sync_service_);

  if (password_sync_enabled_ != sync_enabled_new) {
    password_sync_enabled_ = sync_enabled_new;
    Restart();
  }
}

// PasswordsCounter::PasswordResult

PasswordsCounter::PasswordResult::PasswordResult(const PasswordsCounter* source,
                                                 ResultInt value,
                                                 bool password_sync_enabled)
    : FinishedResult(source, value),
      password_sync_enabled_(password_sync_enabled) {}

PasswordsCounter::PasswordResult::~PasswordResult() {}

}  // namespace browsing_data
