// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A base test fixture for mocking sync and signin infrastructure. Used for
// testing sync-related code.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_USERNAME_TEST_BASE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_USERNAME_TEST_BASE_H_

#include <string>

#include "base/test/scoped_task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class SyncUsernameTestBase : public testing::Test {
 public:
#if defined(OS_CHROMEOS)
  using SigninManagerType = FakeSigninManagerBase;
#else
  using SigninManagerType = FakeSigninManager;
#endif

  SyncUsernameTestBase();
  ~SyncUsernameTestBase() override;

  // Instruct the signin manager to sign in with |email| or out.
  void FakeSigninAs(const std::string& email);

  // Produce a sample PasswordForm.
  static autofill::PasswordForm SimpleGaiaForm(const char* username);
  static autofill::PasswordForm SimpleNonGaiaForm(const char* username);
  static autofill::PasswordForm SimpleNonGaiaForm(const char* username,
                                                  const char* origin);

  // Instruct the sync service to pretend whether or not it is syncing
  // passwords.
  void SetSyncingPasswords(bool syncing_passwords);

  const syncer::SyncService* sync_service() const { return &sync_service_; }

  const SigninManagerBase* signin_manager() { return &signin_manager_; }

  const identity::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

 private:
  class LocalFakeSyncService : public syncer::FakeSyncService {
   public:
    LocalFakeSyncService();
    ~LocalFakeSyncService() override;

    // syncer::SyncService:
    syncer::ModelTypeSet GetPreferredDataTypes() const override;

    void set_syncing_passwords(bool syncing_passwords) {
      syncing_passwords_ = syncing_passwords;
    }

   private:
    bool syncing_passwords_;
  };

  base::test::ScopedTaskEnvironment scoped_task_env_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestSigninClient signin_client_;
  AccountTrackerService account_tracker_;
  FakeProfileOAuth2TokenService token_service_;
  SigninManagerType signin_manager_;
  FakeGaiaCookieManagerService gaia_cookie_manager_service_;
  identity::IdentityTestEnvironment identity_test_env_;
  LocalFakeSyncService sync_service_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_USERNAME_TEST_BASE_H_
