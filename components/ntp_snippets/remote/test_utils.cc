// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/test_utils.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync/driver/fake_sync_service.h"

namespace ntp_snippets {
namespace test {

FakeSyncService::FakeSyncService()
    : can_sync_start_(true),
      is_sync_active_(true),
      configuration_done_(true),
      is_encrypt_everything_enabled_(false),
      active_data_types_(syncer::HISTORY_DELETE_DIRECTIVES) {}

FakeSyncService::~FakeSyncService() = default;

bool FakeSyncService::CanSyncStart() const {
  return can_sync_start_;
}

bool FakeSyncService::IsSyncActive() const {
  return is_sync_active_;
}

bool FakeSyncService::ConfigurationDone() const {
  return configuration_done_;
}

bool FakeSyncService::IsEncryptEverythingEnabled() const {
  return is_encrypt_everything_enabled_;
}

syncer::ModelTypeSet FakeSyncService::GetActiveDataTypes() const {
  return active_data_types_;
}

RemoteSuggestionsTestUtils::RemoteSuggestionsTestUtils()
    : pref_service_(base::MakeUnique<TestingPrefServiceSyncable>()) {
  AccountTrackerService::RegisterPrefs(pref_service_->registry());

#if defined(OS_CHROMEOS)
  SigninManagerBase::RegisterProfilePrefs(pref_service_->registry());
  SigninManagerBase::RegisterPrefs(pref_service_->registry());
#else
  SigninManager::RegisterProfilePrefs(pref_service_->registry());
  SigninManager::RegisterPrefs(pref_service_->registry());
#endif  // OS_CHROMEOS

  token_service_ = base::MakeUnique<FakeProfileOAuth2TokenService>();
  signin_client_ = base::MakeUnique<TestSigninClient>(pref_service_.get());
  account_tracker_ = base::MakeUnique<AccountTrackerService>();
  account_tracker_->Initialize(signin_client_.get());
  fake_sync_service_ = base::MakeUnique<FakeSyncService>();

  ResetSigninManager();
}

RemoteSuggestionsTestUtils::~RemoteSuggestionsTestUtils() = default;

void RemoteSuggestionsTestUtils::ResetSigninManager() {
#if defined(OS_CHROMEOS)
  fake_signin_manager_ = base::MakeUnique<FakeSigninManagerBase>(
      signin_client_.get(), account_tracker_.get());
#else
  fake_signin_manager_ = base::MakeUnique<FakeSigninManager>(
      signin_client_.get(), token_service_.get(), account_tracker_.get(),
      /*cookie_manager_service=*/nullptr);
#endif
}

}  // namespace test
}  // namespace ntp_snippets
