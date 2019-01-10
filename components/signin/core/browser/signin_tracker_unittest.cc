// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_tracker.h"

#include "base/compiler_specific.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/identity_test_environment.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

#if defined(OS_CHROMEOS)
using FakeSigninManagerForTesting = FakeSigninManagerBase;
#else
using FakeSigninManagerForTesting = FakeSigninManager;
#endif  // OS_CHROMEOS

class MockObserver : public SigninTracker::Observer {
 public:
  MockObserver() {}
  ~MockObserver() {}

  MOCK_METHOD1(SigninFailed, void(const GoogleServiceAuthError&));
  MOCK_METHOD0(SigninSuccess, void(void));
  MOCK_METHOD1(AccountAddedToCookie, void(const GoogleServiceAuthError&));
};

}  // namespace

class SigninTrackerTest : public testing::Test {
 public:
  SigninTrackerTest() {
    tracker_ = std::make_unique<SigninTracker>(
        identity_test_env_.identity_manager(), &observer_);
  }

  ~SigninTrackerTest() override { tracker_.reset(); }

  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<SigninTracker> tracker_;
  identity::IdentityTestEnvironment identity_test_env_;
  MockObserver observer_;
};

#if !defined(OS_CHROMEOS)
TEST_F(SigninTrackerTest, SignInFails) {
  const GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  // Signin failure should result in a SigninFailed callback.
  EXPECT_CALL(observer_, SigninSuccess()).Times(0);
  EXPECT_CALL(observer_, SigninFailed(error));

  // Mimic calling IdentityManager::GoogleSigninFailed().
  tracker_->OnPrimaryAccountSigninFailed(error);
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(SigninTrackerTest, SignInSucceeds) {
  EXPECT_CALL(observer_, SigninSuccess());
  EXPECT_CALL(observer_, SigninFailed(_)).Times(0);

  std::string email = "user@gmail.com";
  identity_test_env_.MakePrimaryAccountAvailable(email);
}

#if !defined(OS_CHROMEOS)
TEST_F(SigninTrackerTest, SignInSucceedsWithExistingAccount) {
  EXPECT_CALL(observer_, SigninSuccess());
  EXPECT_CALL(observer_, SigninFailed(_)).Times(0);

  std::string email = "user@gmail.com";
  AccountInfo account_info = identity_test_env_.MakeAccountAvailable(email);
  identity_test_env_.SetPrimaryAccount(account_info.email);
}
#endif
