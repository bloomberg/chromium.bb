// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_auth_attempt.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/signin/easy_unlock_app_manager.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/proximity_auth/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#endif

namespace {

// Fake user ids used in tests.
const char kTestUser1[] = "user1";
const char kTestUser2[] = "user2";

#if defined(OS_CHROMEOS)
const unsigned char kSecret[] = {
    0x7c, 0x85, 0x82, 0x7d, 0x00, 0x1f, 0x6a, 0x29, 0x2f, 0xc4, 0xb5, 0x60,
    0x08, 0x9b, 0xb0, 0x5b
};
#endif

const unsigned char kSessionKey[] = {
    0xc3, 0xd9, 0x83, 0x16, 0x52, 0xde, 0x99, 0xd7, 0x4e, 0x60, 0xf9, 0xec,
    0xa8, 0x9c, 0x0e, 0xbe
};

const unsigned char kWrappedSecret[] = {
    0x3a, 0xea, 0x51, 0xd9, 0x64, 0x64, 0xe1, 0xcd, 0xd8, 0xee, 0x99, 0xf5,
    0xb1, 0xd4, 0x9f, 0xc4, 0x28, 0xd6, 0xfd, 0x69, 0x0b, 0x9e, 0x06, 0x21,
    0xfc, 0x40, 0x1f, 0xeb, 0x75, 0x64, 0x52, 0xd8
};

#if defined(OS_CHROMEOS)
std::string GetSecret() {
  return std::string(reinterpret_cast<const char*>(kSecret),
                     arraysize(kSecret));
}
#endif

std::string GetWrappedSecret() {
  return std::string(reinterpret_cast<const char*>(kWrappedSecret),
                     arraysize(kWrappedSecret));
}

std::string GetSessionKey() {
  return std::string(reinterpret_cast<const char*>(kSessionKey),
                     arraysize(kSessionKey));
}

// Fake app manager used by the EasyUnlockAuthAttempt during tests.
// It tracks screenlockPrivate.onAuthAttempted events.
class FakeAppManager : public EasyUnlockAppManager {
 public:
  FakeAppManager()
      : auth_attempt_count_(0u), auth_attempt_should_fail_(false) {}
  ~FakeAppManager() override {}

  void EnsureReady(const base::Closure& ready_callback) override {
    ADD_FAILURE() << "Not reached";
  }

  void LaunchSetup() override { ADD_FAILURE() << "Not reached"; }

  void LoadApp() override { ADD_FAILURE() << "Not reached"; }

  void DisableAppIfLoaded() override { ADD_FAILURE() << "Not reached"; }

  void ReloadApp() override { ADD_FAILURE() << "Not reached"; }

  bool SendUserUpdatedEvent(const std::string& user_id,
                            bool is_logged_in,
                            bool data_ready) override {
    ADD_FAILURE() << "Not reached";
    return false;
  }

  bool SendAuthAttemptEvent() override {
    ++auth_attempt_count_;
    return !auth_attempt_should_fail_;
  }

  size_t auth_attempt_count() const { return auth_attempt_count_; }

  void set_auth_attempt_should_fail(bool value) {
    auth_attempt_should_fail_ = value;
  }

 private:
  size_t auth_attempt_count_;
  bool auth_attempt_should_fail_;

  DISALLOW_COPY_AND_ASSIGN(FakeAppManager);
};

// Fake lock handler to be used in these tests.
class TestLockHandler : public proximity_auth::ScreenlockBridge::LockHandler {
 public:
  // The state of unlock/signin procedure.
  enum AuthState {
    STATE_NONE,
    STATE_ATTEMPTING_UNLOCK,
    STATE_UNLOCK_CANCELED,
    STATE_UNLOCK_DONE,
    STATE_ATTEMPTING_SIGNIN,
    STATE_SIGNIN_CANCELED,
    STATE_SIGNIN_DONE
  };

  explicit TestLockHandler(const AccountId& account_id)
      : state_(STATE_NONE),
        auth_type_(proximity_auth::mojom::AuthType::USER_CLICK),
        account_id_(account_id) {}

  ~TestLockHandler() override {}

  void set_state(AuthState value) { state_ = value; }
  AuthState state() const { return state_; }

  // Sets the secret that is expected to be sent to |AttemptEasySignin|
  void set_expected_secret(const std::string& value) {
    expected_secret_ = value;
  }

  // Not using |SetAuthType| to make sure it's not called during tests.
  void set_auth_type(proximity_auth::mojom::AuthType value) {
    auth_type_ = value;
  }

  // proximity_auth::ScreenlockBridge::LockHandler implementation:
  void ShowBannerMessage(const base::string16& message) override {
    ADD_FAILURE() << "Should not be reached.";
  }

  void ShowUserPodCustomIcon(
      const AccountId& account_id,
      const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions& icon)
      override {
    ADD_FAILURE() << "Should not be reached.";
  }

  void HideUserPodCustomIcon(const AccountId& account_id) override {
    ADD_FAILURE() << "Should not be reached.";
  }

  void EnableInput() override {
    ASSERT_EQ(STATE_ATTEMPTING_UNLOCK, state_);
    state_ = STATE_UNLOCK_CANCELED;
  }

  void SetAuthType(const AccountId& account_id,
                   proximity_auth::mojom::AuthType auth_type,
                   const base::string16& auth_value) override {
    ADD_FAILURE() << "Should not be reached.";
  }

  proximity_auth::mojom::AuthType GetAuthType(
      const AccountId& account_id) const override {
    return auth_type_;
  }

  ScreenType GetScreenType() const override {
    // Return an arbitrary value; this is not used by the test code.
    return LOCK_SCREEN;
  }

  void Unlock(const AccountId& account_id) override {
    ASSERT_TRUE(account_id_ == account_id)
        << "account_id_=" << account_id_.Serialize()
        << " != " << account_id.Serialize();
    ASSERT_EQ(STATE_ATTEMPTING_UNLOCK, state_);
    state_ = STATE_UNLOCK_DONE;
  }

  void AttemptEasySignin(const AccountId& account_id,
                         const std::string& secret,
                         const std::string& key_label) override {
#if defined(OS_CHROMEOS)
    ASSERT_TRUE(account_id_ == account_id)
        << "account_id_=" << account_id_.Serialize()
        << " != " << account_id.Serialize();

    ASSERT_EQ(STATE_ATTEMPTING_SIGNIN, state_);
    if (secret.empty()) {
      state_ = STATE_SIGNIN_CANCELED;
    } else {
      ASSERT_EQ(expected_secret_, secret);
      ASSERT_EQ(chromeos::EasyUnlockKeyManager::GetKeyLabel(0u), key_label);
      state_ = STATE_SIGNIN_DONE;
    }
#else  // if !defined(OS_CHROMEOS)
    ADD_FAILURE() << "Should not be reached.";
#endif
  }

 private:
  AuthState state_;
  proximity_auth::mojom::AuthType auth_type_;
  const AccountId account_id_;
  std::string expected_secret_;

  DISALLOW_COPY_AND_ASSIGN(TestLockHandler);
};

class EasyUnlockAuthAttemptUnlockTest : public testing::Test {
 public:
  EasyUnlockAuthAttemptUnlockTest() {}
  ~EasyUnlockAuthAttemptUnlockTest() override {}

  void SetUp() override {
    app_manager_.reset(new FakeAppManager());
    auth_attempt_.reset(
        new EasyUnlockAuthAttempt(app_manager_.get(), test_account_id1_,
                                  EasyUnlockAuthAttempt::TYPE_UNLOCK,
                                  EasyUnlockAuthAttempt::FinalizedCallback()));
  }

  void TearDown() override {
    proximity_auth::ScreenlockBridge::Get()->SetLockHandler(NULL);
    auth_attempt_.reset();
  }

 protected:
  void InitScreenLock() {
    lock_handler_.reset(new TestLockHandler(test_account_id1_));
    lock_handler_->set_state(TestLockHandler::STATE_ATTEMPTING_UNLOCK);
    proximity_auth::ScreenlockBridge::Get()->SetLockHandler(
        lock_handler_.get());
  }

  std::unique_ptr<EasyUnlockAuthAttempt> auth_attempt_;
  std::unique_ptr<FakeAppManager> app_manager_;
  std::unique_ptr<TestLockHandler> lock_handler_;

  const AccountId test_account_id1_ = AccountId::FromUserEmail(kTestUser1);
  const AccountId test_account_id2_ = AccountId::FromUserEmail(kTestUser2);

 private:
  DISALLOW_COPY_AND_ASSIGN(EasyUnlockAuthAttemptUnlockTest);
};

TEST_F(EasyUnlockAuthAttemptUnlockTest, StartWhenNotLocked) {
  ASSERT_FALSE(proximity_auth::ScreenlockBridge::Get()->IsLocked());

  EXPECT_FALSE(auth_attempt_->Start());
  EXPECT_EQ(0u, app_manager_->auth_attempt_count());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, StartWhenAuthTypeIsPassword) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  lock_handler_->set_auth_type(
      proximity_auth::mojom::AuthType::OFFLINE_PASSWORD);

  EXPECT_FALSE(auth_attempt_->Start());

  EXPECT_EQ(0u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest,
       StartWhenDispatchingAuthAttemptEventFails) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      proximity_auth::switches::kDisableBluetoothLowEnergyDiscovery);
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  app_manager_->set_auth_attempt_should_fail(true);

  EXPECT_FALSE(auth_attempt_->Start());

  EXPECT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, ResetBeforeFinalizeUnlock) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  auth_attempt_.reset();

  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, FinalizeUnlockFailure) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  auth_attempt_->FinalizeUnlock(test_account_id1_, false);

  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, FinalizeSigninCalled) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  // Wrapped secret and key should be irrelevant in this case.
  auth_attempt_->FinalizeSignin(test_account_id1_, GetWrappedSecret(),
                                GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_UNLOCK_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, UnlockSucceeds) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  auth_attempt_->FinalizeUnlock(test_account_id1_, true);

  ASSERT_EQ(TestLockHandler::STATE_UNLOCK_DONE, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptUnlockTest, FinalizeUnlockCalledForWrongUser) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  auth_attempt_->FinalizeUnlock(test_account_id2_, true);

  // If FinalizeUnlock is called for an incorrect user, it should be ignored
  // rather than cancelling the authentication.
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_UNLOCK, lock_handler_->state());

  // When FinalizeUnlock is called for the correct user, it should work as
  // expected.
  auth_attempt_->FinalizeUnlock(test_account_id1_, true);

  ASSERT_EQ(TestLockHandler::STATE_UNLOCK_DONE, lock_handler_->state());
}

#if defined(OS_CHROMEOS)
class EasyUnlockAuthAttemptSigninTest : public testing::Test {
 public:
  EasyUnlockAuthAttemptSigninTest() {}
  ~EasyUnlockAuthAttemptSigninTest() override {}

  void SetUp() override {
    app_manager_.reset(new FakeAppManager());
    auth_attempt_.reset(
        new EasyUnlockAuthAttempt(app_manager_.get(), test_account_id1_,
                                  EasyUnlockAuthAttempt::TYPE_SIGNIN,
                                  EasyUnlockAuthAttempt::FinalizedCallback()));
  }

  void TearDown() override {
    proximity_auth::ScreenlockBridge::Get()->SetLockHandler(NULL);
    auth_attempt_.reset();
  }

 protected:
  void InitScreenLock() {
    lock_handler_.reset(new TestLockHandler(test_account_id1_));
    lock_handler_->set_state(TestLockHandler::STATE_ATTEMPTING_SIGNIN);
    proximity_auth::ScreenlockBridge::Get()->SetLockHandler(
        lock_handler_.get());
  }

  std::unique_ptr<EasyUnlockAuthAttempt> auth_attempt_;
  std::unique_ptr<FakeAppManager> app_manager_;
  std::unique_ptr<TestLockHandler> lock_handler_;

  const AccountId test_account_id1_ = AccountId::FromUserEmail(kTestUser1);
  const AccountId test_account_id2_ = AccountId::FromUserEmail(kTestUser2);

 private:
  DISALLOW_COPY_AND_ASSIGN(EasyUnlockAuthAttemptSigninTest);
};

TEST_F(EasyUnlockAuthAttemptSigninTest, StartWhenNotLocked) {
  ASSERT_FALSE(proximity_auth::ScreenlockBridge::Get()->IsLocked());

  EXPECT_FALSE(auth_attempt_->Start());
  EXPECT_EQ(0u, app_manager_->auth_attempt_count());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, StartWhenAuthTypeIsPassword) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  lock_handler_->set_auth_type(
      proximity_auth::mojom::AuthType::OFFLINE_PASSWORD);

  EXPECT_FALSE(auth_attempt_->Start());

  EXPECT_EQ(0u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest,
       StartWhenDispatchingAuthAttemptEventFails) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      proximity_auth::switches::kDisableBluetoothLowEnergyDiscovery);
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  app_manager_->set_auth_attempt_should_fail(true);

  EXPECT_FALSE(auth_attempt_->Start());

  EXPECT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, ResetBeforeFinalizeSignin) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_.reset();

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, FinalizeSigninWithEmtpySecret) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeSignin(test_account_id1_, "", GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, FinalizeSigninWithEmtpyKey) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeSignin(test_account_id1_, GetWrappedSecret(), "");

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, SigninSuccess) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  lock_handler_->set_expected_secret(GetSecret());
  auth_attempt_->FinalizeSignin(test_account_id1_, GetWrappedSecret(),
                                GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_DONE, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, WrongWrappedSecret) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeSignin(test_account_id1_, "wrong_secret",
                                GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, InvalidSessionKey) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeSignin(test_account_id1_, GetWrappedSecret(),
                                "invalid_key");

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, FinalizeUnlockCalled) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeUnlock(test_account_id1_, true);

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_CANCELED, lock_handler_->state());
}

TEST_F(EasyUnlockAuthAttemptSigninTest, FinalizeSigninCalledForWrongUser) {
  InitScreenLock();
  ASSERT_TRUE(proximity_auth::ScreenlockBridge::Get()->IsLocked());
  ASSERT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  ASSERT_TRUE(auth_attempt_->Start());

  ASSERT_EQ(1u, app_manager_->auth_attempt_count());
  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  lock_handler_->set_expected_secret(GetSecret());

  auth_attempt_->FinalizeSignin(test_account_id2_, GetWrappedSecret(),
                                GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_ATTEMPTING_SIGNIN, lock_handler_->state());

  auth_attempt_->FinalizeSignin(test_account_id1_, GetWrappedSecret(),
                                GetSessionKey());

  EXPECT_EQ(TestLockHandler::STATE_SIGNIN_DONE, lock_handler_->state());
}
#endif  // defined(OS_CHROMEOS)

}  // namespace
