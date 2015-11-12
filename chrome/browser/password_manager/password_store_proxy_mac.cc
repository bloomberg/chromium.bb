// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_proxy_mac.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/password_manager/password_store_mac.h"
#include "chrome/browser/password_manager/simple_password_store_mac.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/apple_keychain.h"

using password_manager::MigrationStatus;
using password_manager::PasswordStoreChangeList;

PasswordStoreProxyMac::PasswordStoreProxyMac(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_ptr<crypto::AppleKeychain> keychain,
    scoped_ptr<password_manager::LoginDatabase> login_db,
    PrefService* prefs)
    : PasswordStore(main_thread_runner, nullptr),
      login_metadata_db_(login_db.Pass()) {
  DCHECK(login_metadata_db_);
  migration_status_.Init(password_manager::prefs::kKeychainMigrationStatus,
                         prefs);
  if (migration_status_.GetValue() ==
      static_cast<int>(MigrationStatus::MIGRATED)) {
    // The login database will be set later after initialization.
    password_store_simple_ =
        new SimplePasswordStoreMac(main_thread_runner, nullptr, nullptr);
  } else {
    password_store_mac_ =
        new PasswordStoreMac(main_thread_runner, nullptr, keychain.Pass());
  }
}

PasswordStoreProxyMac::~PasswordStoreProxyMac() {
}

bool PasswordStoreProxyMac::Init(
    const syncer::SyncableService::StartSyncFlare& flare) {
  // Set up a background thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  thread_.reset(new base::Thread("Chrome_PasswordStore_Thread"));

  if (!thread_->Start()) {
    thread_.reset();
    return false;
  }

  if (!password_manager::PasswordStore::Init(flare))
    return false;

  return ScheduleTask(
      base::Bind(&PasswordStoreProxyMac::InitOnBackgroundThread, this,
                 static_cast<MigrationStatus>(migration_status_.GetValue())));
}

void PasswordStoreProxyMac::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PasswordStore::Shutdown();
  thread_->Stop();

  // Execute the task which are still pending.
  FlushPendingTasks();

  // Unsubscribe the observer, otherwise it's too late in the destructor.
  migration_status_.Destroy();

  // After the thread has stopped it's impossible to switch from one backend to
  // another. GetBackend() returns the correct result.
  // The backend doesn't need the background thread as PasswordStore::Init() and
  // other public methods were never called on it.
  GetBackend()->Shutdown();
}

scoped_refptr<base::SingleThreadTaskRunner>
PasswordStoreProxyMac::GetBackgroundTaskRunner() {
  return thread_ ? thread_->task_runner() : nullptr;
}

password_manager::PasswordStore* PasswordStoreProxyMac::GetBackend() const {
  if (password_store_mac_)
    return password_store_mac_.get();
  return password_store_simple_.get();
}

void PasswordStoreProxyMac::InitOnBackgroundThread(MigrationStatus status) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (!login_metadata_db_->Init()) {
    login_metadata_db_.reset();
    LOG(ERROR) << "Could not create/open login database.";
  }

  if (status == MigrationStatus::MIGRATED) {
    password_store_simple_->InitWithTaskRunner(GetBackgroundTaskRunner(),
                                               login_metadata_db_.Pass());
  } else {
    password_store_mac_->set_login_metadata_db(login_metadata_db_.get());
    password_store_mac_->InitWithTaskRunner(GetBackgroundTaskRunner());
    if (login_metadata_db_ && (status == MigrationStatus::NOT_STARTED ||
                               status == MigrationStatus::FAILED_ONCE)) {
      // Let's try to migrate the passwords.
      if (password_store_mac_->ImportFromKeychain() ==
          PasswordStoreMac::MIGRATION_OK) {
        status = MigrationStatus::MIGRATED;
        // Switch from |password_store_mac_| to |password_store_simple_|.
        password_store_mac_->set_login_metadata_db(nullptr);
        pending_ui_tasks_.push_back(
            base::Bind(&PasswordStoreMac::Shutdown, password_store_mac_));
        password_store_mac_ = nullptr;
        DCHECK(!password_store_simple_);
        password_store_simple_ = new SimplePasswordStoreMac(
            main_thread_runner_, GetBackgroundTaskRunner(),
            login_metadata_db_.Pass());
      } else {
        status = (status == MigrationStatus::FAILED_ONCE
                      ? MigrationStatus::FAILED_TWICE
                      : MigrationStatus::FAILED_ONCE);
      }
      pending_ui_tasks_.push_back(
          base::Bind(&PasswordStoreProxyMac::UpdateStatusPref, this, status));
    }
  }
  if (!pending_ui_tasks_.empty()) {
    main_thread_runner_->PostTask(
        FROM_HERE, base::Bind(&PasswordStoreProxyMac::FlushPendingTasks, this));
  }
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.KeychainMigration.Status", static_cast<int>(status),
      static_cast<int>(MigrationStatus::MIGRATION_STATUS_COUNT));
  DCHECK(GetBackend());
}

void PasswordStoreProxyMac::UpdateStatusPref(MigrationStatus status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  migration_status_.SetValue(static_cast<int>(status));
}

void PasswordStoreProxyMac::FlushPendingTasks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& task : pending_ui_tasks_)
    task.Run();
  pending_ui_tasks_.clear();
}

void PasswordStoreProxyMac::ReportMetricsImpl(
    const std::string& sync_username,
    bool custom_passphrase_sync_enabled) {
  GetBackend()->ReportMetricsImpl(sync_username,
                                  custom_passphrase_sync_enabled);
}

PasswordStoreChangeList PasswordStoreProxyMac::AddLoginImpl(
    const autofill::PasswordForm& form) {
  return GetBackend()->AddLoginImpl(form);
}

PasswordStoreChangeList PasswordStoreProxyMac::UpdateLoginImpl(
    const autofill::PasswordForm& form) {
  return GetBackend()->UpdateLoginImpl(form);
}

PasswordStoreChangeList PasswordStoreProxyMac::RemoveLoginImpl(
    const autofill::PasswordForm& form) {
  return GetBackend()->RemoveLoginImpl(form);
}

PasswordStoreChangeList PasswordStoreProxyMac::RemoveLoginsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  return GetBackend()->RemoveLoginsCreatedBetweenImpl(delete_begin, delete_end);
}

PasswordStoreChangeList PasswordStoreProxyMac::RemoveLoginsSyncedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  return GetBackend()->RemoveLoginsSyncedBetweenImpl(delete_begin, delete_end);
}

bool PasswordStoreProxyMac::RemoveStatisticsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  return GetBackend()->RemoveStatisticsCreatedBetweenImpl(delete_begin,
                                                          delete_end);
}

ScopedVector<autofill::PasswordForm> PasswordStoreProxyMac::FillMatchingLogins(
    const autofill::PasswordForm& form,
    AuthorizationPromptPolicy prompt_policy) {
  return GetBackend()->FillMatchingLogins(form, prompt_policy);
}

bool PasswordStoreProxyMac::FillAutofillableLogins(
    ScopedVector<autofill::PasswordForm>* forms) {
  return GetBackend()->FillAutofillableLogins(forms);
}

bool PasswordStoreProxyMac::FillBlacklistLogins(
    ScopedVector<autofill::PasswordForm>* forms) {
  return GetBackend()->FillBlacklistLogins(forms);
}

void PasswordStoreProxyMac::AddSiteStatsImpl(
    const password_manager::InteractionsStats& stats) {
  GetBackend()->AddSiteStatsImpl(stats);
}

void PasswordStoreProxyMac::RemoveSiteStatsImpl(const GURL& origin_domain) {
  GetBackend()->RemoveSiteStatsImpl(origin_domain);
}

ScopedVector<password_manager::InteractionsStats>
PasswordStoreProxyMac::GetSiteStatsImpl(const GURL& origin_domain) {
  return GetBackend()->GetSiteStatsImpl(origin_domain);
}
