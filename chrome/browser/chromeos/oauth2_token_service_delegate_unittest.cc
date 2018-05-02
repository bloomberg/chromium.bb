// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/oauth2_token_service_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/account_manager/account_manager.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

using account_manager::AccountType::ACCOUNT_TYPE_GAIA;
using account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY;

class AuthErrorObserver : public OAuth2TokenService::Observer {
 public:
  void OnAuthErrorChanged(const std::string& account_id,
                          const GoogleServiceAuthError& auth_error) override {
    last_err_account_id_ = account_id;
    last_err_ = auth_error;
  }

  std::string last_err_account_id_;
  GoogleServiceAuthError last_err_;
};

}  // namespace

class CrOSOAuthDelegateTest : public testing::Test {
 public:
  CrOSOAuthDelegateTest() = default;
  ~CrOSOAuthDelegateTest() override = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());

    account_manager_.Initialize(tmp_dir_.GetPath());
    scoped_task_environment_.RunUntilIdle();

    pref_service_.registry()->RegisterListPref(
        AccountTrackerService::kAccountInfoPref);
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kAccountIdMigrationState,
        AccountTrackerService::MIGRATION_NOT_STARTED);
    client_.reset(new TestSigninClient(&pref_service_));
    client_->SetURLRequestContext(new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get()));

    account_tracker_service_.Initialize(client_.get());

    account_info_.email = "user@gmail.com";
    account_info_.gaia = "111";
    account_info_.full_name = "name";
    account_info_.given_name = "name";
    account_info_.hosted_domain = "example.com";
    account_info_.locale = "en";
    account_info_.picture_url = "https://example.com";
    account_info_.is_child_account = false;
    account_info_.account_id = account_tracker_service_.PickAccountIdForAccount(
        account_info_.gaia, account_info_.email);

    ASSERT_TRUE(account_info_.IsValid());
    account_tracker_service_.SeedAccountInfo(account_info_);

    delegate_ = std::make_unique<ChromeOSOAuth2TokenServiceDelegate>(
        &account_tracker_service_, &account_manager_);
  }

  // Check base/test/scoped_task_environment.h. This must be the first member /
  // declared before any member that cares about tasks.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  AccountInfo account_info_;
  AccountManager account_manager_;
  std::unique_ptr<ChromeOSOAuth2TokenServiceDelegate> delegate_;

 private:
  base::ScopedTempDir tmp_dir_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<TestSigninClient> client_;
  AccountTrackerService account_tracker_service_;

  DISALLOW_COPY_AND_ASSIGN(CrOSOAuthDelegateTest);
};

TEST_F(CrOSOAuthDelegateTest, RefreshTokenIsAvailableForGaiaAccounts) {
  EXPECT_EQ(OAuth2TokenServiceDelegate::LoadCredentialsState::
                LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS,
            delegate_->GetLoadCredentialsState());

  EXPECT_FALSE(delegate_->RefreshTokenIsAvailable(account_info_.account_id));

  const AccountManager::AccountKey account_key{account_info_.gaia,
                                               ACCOUNT_TYPE_GAIA};
  account_manager_.UpsertToken(account_key, "token");

  EXPECT_TRUE(delegate_->RefreshTokenIsAvailable(account_info_.account_id));
}

TEST_F(CrOSOAuthDelegateTest, ObserversAreNotifiedOnAuthErrorChange) {
  AuthErrorObserver observer;
  auto error =
      GoogleServiceAuthError(GoogleServiceAuthError::State::SERVICE_ERROR);
  delegate_->AddObserver(&observer);

  delegate_->UpdateAuthError(account_info_.account_id, error);
  EXPECT_EQ(error, delegate_->GetAuthError(account_info_.account_id));
  EXPECT_EQ(account_info_.account_id, observer.last_err_account_id_);
  EXPECT_EQ(error, observer.last_err_);

  delegate_->RemoveObserver(&observer);
}

TEST_F(CrOSOAuthDelegateTest, GetAccountsShouldNotReturnAdAccounts) {
  EXPECT_TRUE(delegate_->GetAccounts().empty());

  // Insert an Active Directory account into AccountManager.
  AccountManager::AccountKey ad_account_key{"111",
                                            ACCOUNT_TYPE_ACTIVE_DIRECTORY};
  account_manager_.UpsertToken(ad_account_key, "" /* token */);

  // OAuth delegate should not return Active Directory accounts.
  EXPECT_TRUE(delegate_->GetAccounts().empty());
}

TEST_F(CrOSOAuthDelegateTest, GetAccountsReturnsGaiaAccounts) {
  EXPECT_TRUE(delegate_->GetAccounts().empty());

  AccountManager::AccountKey gaia_account_key{"111", ACCOUNT_TYPE_GAIA};
  account_manager_.UpsertToken(gaia_account_key, "token");

  std::vector<std::string> accounts = delegate_->GetAccounts();
  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ(account_info_.account_id, accounts[0]);
}

TEST_F(CrOSOAuthDelegateTest, UpdateCredentialsSucceeds) {
  EXPECT_TRUE(delegate_->GetAccounts().empty());

  delegate_->UpdateCredentials(account_info_.account_id, "token");

  std::vector<std::string> accounts = delegate_->GetAccounts();
  EXPECT_EQ(1UL, accounts.size());
  EXPECT_EQ(account_info_.account_id, accounts[0]);
}

}  // namespace chromeos
