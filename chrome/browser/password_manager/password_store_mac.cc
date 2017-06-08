// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_mac.h"

#include "base/metrics/histogram_macros.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/browser_thread.h"

using password_manager::MigrationStatus;

PasswordStoreMac::PasswordStoreMac(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    std::unique_ptr<password_manager::LoginDatabase> login_db,
    PrefService* prefs)
    : PasswordStoreDefault(main_thread_runner, nullptr, std::move(login_db)) {
  migration_status_.Init(password_manager::prefs::kKeychainMigrationStatus,
                         prefs);
}

bool PasswordStoreMac::Init(
    const syncer::SyncableService::StartSyncFlare& flare,
    PrefService* prefs) {
  // Set up a background thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  thread_.reset(new base::Thread("Chrome_PasswordStore_Thread"));

  if (!thread_->Start()) {
    thread_.reset();
    return false;
  }

  if (PasswordStoreDefault::Init(flare, prefs)) {
    return ScheduleTask(
        base::Bind(&PasswordStoreMac::InitOnBackgroundThread, this,
                   static_cast<MigrationStatus>(migration_status_.GetValue())));
  }

  return false;
}

void PasswordStoreMac::ShutdownOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PasswordStoreDefault::ShutdownOnUIThread();
  thread_->Stop();

  // Unsubscribe the observer, otherwise it's too late in the destructor.
  migration_status_.Destroy();
}

scoped_refptr<base::SingleThreadTaskRunner>
PasswordStoreMac::GetBackgroundTaskRunner() {
  return thread_ ? thread_->task_runner() : nullptr;
}

PasswordStoreMac::~PasswordStoreMac() = default;

void PasswordStoreMac::InitOnBackgroundThread(MigrationStatus status) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());

  if (login_db() && (status == MigrationStatus::NOT_STARTED ||
                     status == MigrationStatus::FAILED_ONCE ||
                     status == MigrationStatus::FAILED_TWICE)) {
    // Migration isn't possible due to Chrome changing the certificate. Just
    // drop the entries in the DB because they don't have passwords anyway.
    login_db()->RemoveLoginsCreatedBetween(base::Time(), base::Time());
    status = MigrationStatus::MIGRATION_STOPPED;
    main_thread_runner_->PostTask(
        FROM_HERE,
        base::Bind(&PasswordStoreMac::UpdateStatusPref, this, status));
  }

  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.KeychainMigration.Status", static_cast<int>(status),
      static_cast<int>(MigrationStatus::MIGRATION_STATUS_COUNT));
}

void PasswordStoreMac::UpdateStatusPref(MigrationStatus status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The method can be called after ShutdownOnUIThread().
  if (migration_status_.prefs())
    migration_status_.SetValue(static_cast<int>(status));
}
