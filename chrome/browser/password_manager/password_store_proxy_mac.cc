// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_proxy_mac.h"

#include "chrome/browser/password_manager/password_store_mac.h"
#include "chrome/browser/password_manager/simple_password_store_mac.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/apple_keychain.h"

using password_manager::PasswordStoreChangeList;

PasswordStoreProxyMac::PasswordStoreProxyMac(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_ptr<crypto::AppleKeychain> keychain,
    scoped_ptr<password_manager::LoginDatabase> login_db)
    : PasswordStore(main_thread_runner, nullptr),
      login_metadata_db_(login_db.Pass()) {
  DCHECK(login_metadata_db_);
  // TODO(vasilii): for now the class is just a wrapper around PasswordStoreMac.
  password_store_mac_ =
      new PasswordStoreMac(main_thread_runner, nullptr, keychain.Pass());
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

  ScheduleTask(
      base::Bind(&PasswordStoreProxyMac::InitOnBackgroundThread, this));
  password_store_mac_->InitWithTaskRunner(GetBackgroundTaskRunner());
  return password_manager::PasswordStore::Init(flare);
}

void PasswordStoreProxyMac::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PasswordStore::Shutdown();
  GetBackend()->Shutdown();
  thread_->Stop();
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

void PasswordStoreProxyMac::InitOnBackgroundThread() {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  if (!login_metadata_db_->Init()) {
    login_metadata_db_.reset();
    LOG(ERROR) << "Could not create/open login database.";
    return;
  }
  if (password_store_mac_)
    password_store_mac_->set_login_metadata_db(login_metadata_db_.get());
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

scoped_ptr<password_manager::InteractionsStats>
PasswordStoreProxyMac::GetSiteStatsImpl(const GURL& origin_domain) {
  return GetBackend()->GetSiteStatsImpl(origin_domain);
}
