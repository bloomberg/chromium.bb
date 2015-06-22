// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_proxy_mac.h"

#include "chrome/browser/password_manager/password_store_mac.h"
#include "chrome/browser/password_manager/simple_password_store_mac.h"
#include "crypto/apple_keychain.h"

using password_manager::PasswordStoreChangeList;

PasswordStoreProxyMac::PasswordStoreProxyMac(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_ptr<crypto::AppleKeychain> keychain,
    scoped_ptr<password_manager::LoginDatabase> login_db)
    : PasswordStore(main_thread_runner, nullptr) {
  // TODO(vasilii): for now the class is just a wrapper around PasswordStoreMac.
  password_store_mac_ = new PasswordStoreMac(main_thread_runner, nullptr,
                                             keychain.Pass(), login_db.Pass());
}

PasswordStoreProxyMac::~PasswordStoreProxyMac() {
}

bool PasswordStoreProxyMac::Init(
    const syncer::SyncableService::StartSyncFlare& flare) {
  return GetBackend()->Init(flare);
}

void PasswordStoreProxyMac::Shutdown() {
  return GetBackend()->Shutdown();
}

password_manager::PasswordStore* PasswordStoreProxyMac::GetBackend() const {
  if (password_store_mac_)
    return password_store_mac_.get();
  return password_store_simple_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
PasswordStoreProxyMac::GetBackgroundTaskRunner() {
  return GetBackend()->GetBackgroundTaskRunner();
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

void PasswordStoreProxyMac::GetAutofillableLoginsImpl(
    scoped_ptr<PasswordStore::GetLoginsRequest> request) {
  GetBackend()->GetAutofillableLoginsImpl(request.Pass());
}

void PasswordStoreProxyMac::GetBlacklistLoginsImpl(
    scoped_ptr<PasswordStore::GetLoginsRequest> request) {
  GetBackend()->GetBlacklistLoginsImpl(request.Pass());
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
