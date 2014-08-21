// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth/chrome_cryptohome_authenticator.h"

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/mock_owner_key_util.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/mock_auth_status_consumer.h"
#include "chromeos/login/auth/mock_url_fetchers.h"
#include "chromeos/login/auth/test_attempt_state.h"
#include "chromeos/login/auth/user_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "google_apis/gaia/mock_url_fetcher_factory.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "url/gurl.h"

using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace chromeos {

namespace {

// An owner key in PKCS#8 PrivateKeyInfo for testing owner checks.
const uint8 kOwnerPrivateKey[] = {
    0x30, 0x82, 0x01, 0x53, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x01, 0x3d, 0x30, 0x82, 0x01, 0x39, 0x02, 0x01, 0x00, 0x02, 0x41, 0x00,
    0xb4, 0xf5, 0xab, 0xfe, 0xd8, 0xf1, 0xcb, 0x5f, 0x8f, 0x48, 0x3e, 0xdf,
    0x40, 0x8e, 0x2b, 0x15, 0x43, 0x6c, 0x67, 0x74, 0xa2, 0xcb, 0xe4, 0xf3,
    0xec, 0xab, 0x41, 0x57, 0x1d, 0x5f, 0xed, 0xcf, 0x09, 0xf4, 0xcc, 0xbb,
    0x52, 0x52, 0xe8, 0x46, 0xf5, 0xc5, 0x01, 0xa3, 0xd8, 0x24, 0xc0, 0x15,
    0xc5, 0x65, 0x50, 0x7d, 0xbd, 0x4e, 0x81, 0xb2, 0x28, 0x38, 0xf9, 0x3d,
    0x3e, 0x2a, 0x68, 0xf7, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x40, 0x40,
    0xc7, 0xb5, 0xb3, 0xbc, 0xac, 0x0a, 0x77, 0x02, 0x0f, 0x05, 0xda, 0xdb,
    0xfc, 0x48, 0xf6, 0x0a, 0xb5, 0xf2, 0xef, 0x31, 0x1c, 0x36, 0xb1, 0x0f,
    0xa7, 0x5a, 0xf3, 0xb9, 0xa3, 0x4e, 0xb8, 0xf6, 0x10, 0xfe, 0x25, 0x7b,
    0x36, 0xb4, 0x1b, 0x80, 0xe3, 0x92, 0x37, 0x83, 0xf0, 0x43, 0xb3, 0x00,
    0xa6, 0x53, 0xc6, 0x1b, 0x7e, 0x4b, 0xb0, 0x33, 0xd4, 0xe1, 0x03, 0xc4,
    0xaa, 0xbc, 0x89, 0x02, 0x21, 0x00, 0xde, 0xc8, 0x8d, 0x10, 0xbc, 0xf3,
    0x43, 0x49, 0x1f, 0x07, 0xf7, 0x12, 0xeb, 0x0a, 0x90, 0xab, 0xb9, 0xaa,
    0x81, 0xb5, 0x54, 0x71, 0xf4, 0x2e, 0xc4, 0x44, 0xec, 0xff, 0x7d, 0xff,
    0xe8, 0xa5, 0x02, 0x21, 0x00, 0xcf, 0xf0, 0xbe, 0xa6, 0xde, 0x9c, 0x70,
    0xed, 0xf0, 0xc3, 0x18, 0x9b, 0xca, 0xe5, 0x7c, 0x4b, 0x9b, 0xf5, 0x12,
    0x5d, 0x86, 0xbe, 0x8d, 0xf1, 0xbc, 0x2c, 0x79, 0x59, 0xf5, 0xff, 0xbc,
    0x6b, 0x02, 0x20, 0x7c, 0x09, 0x1c, 0xc1, 0x1c, 0xf2, 0x33, 0x9c, 0x1a,
    0x72, 0xcc, 0xd4, 0xf3, 0x97, 0xc6, 0x44, 0x55, 0xf2, 0xe0, 0x94, 0x9c,
    0x97, 0x75, 0x64, 0x34, 0x52, 0x4b, 0xc1, 0x53, 0xdd, 0x8f, 0x21, 0x02,
    0x20, 0x0e, 0xef, 0x48, 0x92, 0x2d, 0x9c, 0xe8, 0xd3, 0x7e, 0x1e, 0x55,
    0x0f, 0x23, 0x74, 0x76, 0x07, 0xec, 0x2c, 0x9e, 0xe4, 0x0e, 0xc0, 0x72,
    0xeb, 0x70, 0xcb, 0x74, 0xef, 0xcc, 0x26, 0x50, 0xff, 0x02, 0x20, 0x29,
    0x32, 0xd0, 0xbf, 0x11, 0xf2, 0xbf, 0x54, 0xfd, 0x6d, 0xf2, 0x1c, 0xbe,
    0x50, 0x18, 0x62, 0x6d, 0x23, 0xe4, 0x26, 0x03, 0x8b, 0xb3, 0x42, 0x24,
    0x7e, 0x68, 0x37, 0x26, 0xda, 0xb9, 0x87};

// The public key alone matcing kOwnerPrivateKey.
const uint8 kOwnerPublicKey[] = {
    0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
    0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x4b, 0x00, 0x30, 0x48, 0x02, 0x41,
    0x00, 0xb4, 0xf5, 0xab, 0xfe, 0xd8, 0xf1, 0xcb, 0x5f, 0x8f, 0x48, 0x3e,
    0xdf, 0x40, 0x8e, 0x2b, 0x15, 0x43, 0x6c, 0x67, 0x74, 0xa2, 0xcb, 0xe4,
    0xf3, 0xec, 0xab, 0x41, 0x57, 0x1d, 0x5f, 0xed, 0xcf, 0x09, 0xf4, 0xcc,
    0xbb, 0x52, 0x52, 0xe8, 0x46, 0xf5, 0xc5, 0x01, 0xa3, 0xd8, 0x24, 0xc0,
    0x15, 0xc5, 0x65, 0x50, 0x7d, 0xbd, 0x4e, 0x81, 0xb2, 0x28, 0x38, 0xf9,
    0x3d, 0x3e, 0x2a, 0x68, 0xf7, 0x02, 0x03, 0x01, 0x00, 0x01};

std::vector<uint8> GetOwnerPublicKey() {
  return std::vector<uint8>(kOwnerPublicKey,
                            kOwnerPublicKey + arraysize(kOwnerPublicKey));
}

scoped_ptr<crypto::RSAPrivateKey> CreateOwnerKeyInSlot(PK11SlotInfo* slot) {
  const std::vector<uint8> key(kOwnerPrivateKey,
                               kOwnerPrivateKey + arraysize(kOwnerPrivateKey));
  return make_scoped_ptr(
      crypto::RSAPrivateKey::CreateSensitiveFromPrivateKeyInfo(slot, key));
}

}  // namespace

class CryptohomeAuthenticatorTest : public testing::Test {
 public:
  CryptohomeAuthenticatorTest()
      : user_context_("me@nowhere.org"),
        user_manager_(new FakeUserManager()),
        user_manager_enabler_(user_manager_),
        mock_caller_(NULL),
        owner_key_util_(new MockOwnerKeyUtil) {
    user_context_.SetKey(Key("fakepass"));
    user_context_.SetUserIDHash("me_nowhere_com_hash");
    const user_manager::User* user =
        user_manager_->AddUser(user_context_.GetUserID());
    profile_.set_profile_name(user_context_.GetUserID());

    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, &profile_);

    transformed_key_ = *user_context_.GetKey();
    transformed_key_.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                               SystemSaltGetter::ConvertRawSaltToHexString(
                                   FakeCryptohomeClient::GetStubSystemSalt()));
  }

  virtual ~CryptohomeAuthenticatorTest() {}

  virtual void SetUp() {
    CommandLine::ForCurrentProcess()->AppendSwitch(switches::kLoginManager);

    mock_caller_ = new cryptohome::MockAsyncMethodCaller;
    cryptohome::AsyncMethodCaller::InitializeForTesting(mock_caller_);

    FakeDBusThreadManager* fake_dbus_thread_manager = new FakeDBusThreadManager;
    fake_cryptohome_client_ = new FakeCryptohomeClient;
    fake_dbus_thread_manager->SetCryptohomeClient(
        scoped_ptr<CryptohomeClient>(fake_cryptohome_client_));
    DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager);

    SystemSaltGetter::Initialize();

    OwnerSettingsService::SetOwnerKeyUtilForTesting(owner_key_util_);

    auth_ = new ChromeCryptohomeAuthenticator(&consumer_);
    state_.reset(new TestAttemptState(user_context_, false));
  }

  // Tears down the test fixture.
  virtual void TearDown() {
    OwnerSettingsService::SetOwnerKeyUtilForTesting(NULL);
    SystemSaltGetter::Shutdown();
    DBusThreadManager::Shutdown();

    cryptohome::AsyncMethodCaller::Shutdown();
    mock_caller_ = NULL;
  }

  base::FilePath PopulateTempFile(const char* data, int data_len) {
    base::FilePath out;
    FILE* tmp_file = base::CreateAndOpenTemporaryFile(&out);
    EXPECT_NE(tmp_file, static_cast<FILE*>(NULL));
    EXPECT_EQ(base::WriteFile(out, data, data_len), data_len);
    EXPECT_TRUE(base::CloseFile(tmp_file));
    return out;
  }

  // Allow test to fail and exit gracefully, even if OnAuthFailure()
  // wasn't supposed to happen.
  void FailOnLoginFailure() {
    ON_CALL(consumer_, OnAuthFailure(_))
        .WillByDefault(Invoke(MockAuthStatusConsumer::OnFailQuitAndFail));
  }

  // Allow test to fail and exit gracefully, even if
  // OnRetailModeAuthSuccess() wasn't supposed to happen.
  void FailOnRetailModeLoginSuccess() {
    ON_CALL(consumer_, OnRetailModeAuthSuccess(_)).WillByDefault(
        Invoke(MockAuthStatusConsumer::OnRetailModeSuccessQuitAndFail));
  }

  // Allow test to fail and exit gracefully, even if OnAuthSuccess()
  // wasn't supposed to happen.
  void FailOnLoginSuccess() {
    ON_CALL(consumer_, OnAuthSuccess(_))
        .WillByDefault(Invoke(MockAuthStatusConsumer::OnSuccessQuitAndFail));
  }

  // Allow test to fail and exit gracefully, even if
  // OnOffTheRecordAuthSuccess() wasn't supposed to happen.
  void FailOnGuestLoginSuccess() {
    ON_CALL(consumer_, OnOffTheRecordAuthSuccess()).WillByDefault(
        Invoke(MockAuthStatusConsumer::OnGuestSuccessQuitAndFail));
  }

  void ExpectLoginFailure(const AuthFailure& failure) {
    EXPECT_CALL(consumer_, OnAuthFailure(failure))
        .WillOnce(Invoke(MockAuthStatusConsumer::OnFailQuit))
        .RetiresOnSaturation();
  }

  void ExpectRetailModeLoginSuccess() {
    EXPECT_CALL(consumer_, OnRetailModeAuthSuccess(_))
        .WillOnce(Invoke(MockAuthStatusConsumer::OnRetailModeSuccessQuit))
        .RetiresOnSaturation();
  }

  void ExpectLoginSuccess(const UserContext& user_context) {
    EXPECT_CALL(consumer_, OnAuthSuccess(user_context))
        .WillOnce(Invoke(MockAuthStatusConsumer::OnSuccessQuit))
        .RetiresOnSaturation();
  }

  void ExpectGuestLoginSuccess() {
    EXPECT_CALL(consumer_, OnOffTheRecordAuthSuccess())
        .WillOnce(Invoke(MockAuthStatusConsumer::OnGuestSuccessQuit))
        .RetiresOnSaturation();
  }

  void ExpectPasswordChange() {
    EXPECT_CALL(consumer_, OnPasswordChangeDetected())
        .WillOnce(Invoke(MockAuthStatusConsumer::OnMigrateQuit))
        .RetiresOnSaturation();
  }

  void RunResolve(CryptohomeAuthenticator* auth) {
    auth->Resolve();
    base::MessageLoop::current()->RunUntilIdle();
  }

  void SetAttemptState(CryptohomeAuthenticator* auth, TestAttemptState* state) {
    auth->set_attempt_state(state);
  }

  CryptohomeAuthenticator::AuthState SetAndResolveState(
      CryptohomeAuthenticator* auth,
      TestAttemptState* state) {
    auth->set_attempt_state(state);
    return auth->ResolveState();
  }

  void SetOwnerState(bool owner_check_finished, bool check_result) {
    auth_->SetOwnerState(owner_check_finished, check_result);
  }

  content::TestBrowserThreadBundle thread_bundle_;

  UserContext user_context_;
  Key transformed_key_;

  ScopedDeviceSettingsTestHelper device_settings_test_helper_;
  ScopedTestCrosSettings test_cros_settings_;

  TestingProfile profile_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  FakeUserManager* user_manager_;
  ScopedUserManagerEnabler user_manager_enabler_;

  cryptohome::MockAsyncMethodCaller* mock_caller_;

  MockAuthStatusConsumer consumer_;

  scoped_refptr<CryptohomeAuthenticator> auth_;
  scoped_ptr<TestAttemptState> state_;
  FakeCryptohomeClient* fake_cryptohome_client_;

  scoped_refptr<MockOwnerKeyUtil> owner_key_util_;
};

TEST_F(CryptohomeAuthenticatorTest, OnAuthSuccess) {
  EXPECT_CALL(consumer_, OnAuthSuccess(user_context_))
      .Times(1)
      .RetiresOnSaturation();

  SetAttemptState(auth_.get(), state_.release());
  auth_->OnAuthSuccess();
}

TEST_F(CryptohomeAuthenticatorTest, OnPasswordChangeDetected) {
  EXPECT_CALL(consumer_, OnPasswordChangeDetected())
      .Times(1)
      .RetiresOnSaturation();
  SetAttemptState(auth_.get(), state_.release());
  auth_->OnPasswordChangeDetected();
}

TEST_F(CryptohomeAuthenticatorTest, ResolveNothingDone) {
  EXPECT_EQ(CryptohomeAuthenticator::CONTINUE,
            SetAndResolveState(auth_.get(), state_.release()));
}

TEST_F(CryptohomeAuthenticatorTest, ResolvePossiblePwChangeToFailedMount) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected.
  state_->PresetCryptohomeStatus(false, cryptohome::MOUNT_ERROR_KEY_FAILURE);

  // When there is no online attempt and online results, POSSIBLE_PW_CHANGE
  EXPECT_EQ(CryptohomeAuthenticator::FAILED_MOUNT,
            SetAndResolveState(auth_.get(), state_.release()));
}

TEST_F(CryptohomeAuthenticatorTest, ResolveNeedOldPw) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because of unmatched key; additionally,
  // an online auth attempt has completed successfully.
  state_->PresetCryptohomeStatus(false, cryptohome::MOUNT_ERROR_KEY_FAILURE);
  state_->PresetOnlineLoginStatus(AuthFailure::AuthFailureNone());

  EXPECT_EQ(CryptohomeAuthenticator::NEED_OLD_PW,
            SetAndResolveState(auth_.get(), state_.release()));
}

TEST_F(CryptohomeAuthenticatorTest, ResolveOwnerNeededDirectFailedMount) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and succeeded but we are in safe mode and the current user is not owner.
  // This is a high level test to verify the proper transitioning in this mode
  // only. It is not testing that we properly verify that the user is an owner
  // or that we really are in "safe-mode".
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  SetOwnerState(true, false);

  EXPECT_EQ(CryptohomeAuthenticator::OWNER_REQUIRED,
            SetAndResolveState(auth_.get(), state_.release()));
}

TEST_F(CryptohomeAuthenticatorTest, ResolveOwnerNeededMount) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and succeeded but we are in safe mode and the current user is not owner.
  // This test will check that the "safe-mode" policy is not set and will let
  // the mount finish successfully.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  SetOwnerState(false, false);
  EXPECT_EQ(CryptohomeAuthenticator::OFFLINE_LOGIN,
            SetAndResolveState(auth_.get(), state_.release()));
}

// Test the case that login switches to SafeMode and a User that is not the
// owner tries to log in. The login should fail because of the missing owner
// private key.
TEST_F(CryptohomeAuthenticatorTest, ResolveOwnerNeededFailedMount) {
  crypto::ScopedTestNSSChromeOSUser user_slot(user_context_.GetUserIDHash());
  owner_key_util_->SetPublicKey(GetOwnerPublicKey());

  profile_manager_.reset(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  ASSERT_TRUE(profile_manager_->SetUp());

  FailOnLoginSuccess();  // Set failing on success as the default...
  AuthFailure failure = AuthFailure(AuthFailure::OWNER_REQUIRED);
  ExpectLoginFailure(failure);

  // Set up state as though a cryptohome mount attempt has occurred
  // and succeeded but we are in safe mode and the current user is not owner.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  SetOwnerState(false, false);
  // Remove the real DeviceSettingsProvider and replace it with a stub.
  CrosSettingsProvider* device_settings_provider =
      CrosSettings::Get()->GetProvider(chromeos::kReportDeviceVersionInfo);
  EXPECT_TRUE(device_settings_provider != NULL);
  EXPECT_TRUE(
      CrosSettings::Get()->RemoveSettingsProvider(device_settings_provider));
  StubCrosSettingsProvider stub_settings_provider;
  CrosSettings::Get()->AddSettingsProvider(&stub_settings_provider);
  CrosSettings::Get()->SetBoolean(kPolicyMissingMitigationMode, true);

  // Initialize login state for this test to verify the login state is changed
  // to SAFE_MODE.
  LoginState::Initialize();

  EXPECT_EQ(CryptohomeAuthenticator::CONTINUE,
            SetAndResolveState(auth_.get(), state_.release()));
  EXPECT_TRUE(LoginState::Get()->IsInSafeMode());

  // Flush all the pending operations. The operations should induce an owner
  // verification.
  device_settings_test_helper_.Flush();

  state_.reset(new TestAttemptState(user_context_, false));
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);

  // The owner key util should not have found the owner key, so login should
  // not be allowed.
  EXPECT_EQ(CryptohomeAuthenticator::OWNER_REQUIRED,
            SetAndResolveState(auth_.get(), state_.release()));
  EXPECT_TRUE(LoginState::Get()->IsInSafeMode());

  // Unset global objects used by this test.
  fake_cryptohome_client_->set_unmount_result(true);
  LoginState::Shutdown();
  EXPECT_TRUE(
      CrosSettings::Get()->RemoveSettingsProvider(&stub_settings_provider));
  CrosSettings::Get()->AddSettingsProvider(device_settings_provider);
}

// Test the case that login switches to SafeMode and the Owner logs in, which
// should lead to a successful login.
TEST_F(CryptohomeAuthenticatorTest, ResolveOwnerNeededSuccess) {
  crypto::ScopedTestNSSChromeOSUser test_user_db(user_context_.GetUserIDHash());
  owner_key_util_->SetPublicKey(GetOwnerPublicKey());

  crypto::ScopedPK11Slot user_slot(
      crypto::GetPublicSlotForChromeOSUser(user_context_.GetUserIDHash()));
  CreateOwnerKeyInSlot(user_slot.get());

  profile_manager_.reset(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  ASSERT_TRUE(profile_manager_->SetUp());

  ExpectLoginSuccess(user_context_);

  // Set up state as though a cryptohome mount attempt has occurred
  // and succeeded but we are in safe mode and the current user is not owner.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  SetOwnerState(false, false);
  // Remove the real DeviceSettingsProvider and replace it with a stub.
  CrosSettingsProvider* device_settings_provider =
      CrosSettings::Get()->GetProvider(chromeos::kReportDeviceVersionInfo);
  EXPECT_TRUE(device_settings_provider != NULL);
  EXPECT_TRUE(
      CrosSettings::Get()->RemoveSettingsProvider(device_settings_provider));
  StubCrosSettingsProvider stub_settings_provider;
  CrosSettings::Get()->AddSettingsProvider(&stub_settings_provider);
  CrosSettings::Get()->SetBoolean(kPolicyMissingMitigationMode, true);

  // Initialize login state for this test to verify the login state is changed
  // to SAFE_MODE.
  LoginState::Initialize();

  EXPECT_EQ(CryptohomeAuthenticator::CONTINUE,
            SetAndResolveState(auth_.get(), state_.release()));
  EXPECT_TRUE(LoginState::Get()->IsInSafeMode());

  // Flush all the pending operations. The operations should induce an owner
  // verification.
  device_settings_test_helper_.Flush();

  state_.reset(new TestAttemptState(user_context_, false));
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);

  // The owner key util should find the owner key, so login should succeed.
  EXPECT_EQ(CryptohomeAuthenticator::OFFLINE_LOGIN,
            SetAndResolveState(auth_.get(), state_.release()));
  EXPECT_TRUE(LoginState::Get()->IsInSafeMode());

  // Unset global objects used by this test.
  fake_cryptohome_client_->set_unmount_result(true);
  LoginState::Shutdown();
  EXPECT_TRUE(
      CrosSettings::Get()->RemoveSettingsProvider(&stub_settings_provider));
  CrosSettings::Get()->AddSettingsProvider(device_settings_provider);
}

TEST_F(CryptohomeAuthenticatorTest, DriveFailedMount) {
  FailOnLoginSuccess();
  ExpectLoginFailure(AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));

  // Set up state as though a cryptohome mount attempt has occurred
  // and failed.
  state_->PresetCryptohomeStatus(false, cryptohome::MOUNT_ERROR_NONE);
  SetAttemptState(auth_.get(), state_.release());

  RunResolve(auth_.get());
}

TEST_F(CryptohomeAuthenticatorTest, DriveGuestLogin) {
  ExpectGuestLoginSuccess();
  FailOnLoginFailure();

  // Set up mock async method caller to respond as though a tmpfs mount
  // attempt has occurred and succeeded.
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMountGuest(_)).Times(1).RetiresOnSaturation();

  auth_->LoginOffTheRecord();
  base::MessageLoop::current()->Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveGuestLoginButFail) {
  FailOnGuestLoginSuccess();
  ExpectLoginFailure(AuthFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS));

  // Set up mock async method caller to respond as though a tmpfs mount
  // attempt has occurred and failed.
  mock_caller_->SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMountGuest(_)).Times(1).RetiresOnSaturation();

  auth_->LoginOffTheRecord();
  base::MessageLoop::current()->Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveRetailModeUserLogin) {
  ExpectRetailModeLoginSuccess();
  FailOnLoginFailure();

  // Set up mock async method caller to respond as though a tmpfs mount
  // attempt has occurred and succeeded.
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMountGuest(_)).Times(1).RetiresOnSaturation();

  auth_->LoginRetailMode();
  base::MessageLoop::current()->Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveRetailModeLoginButFail) {
  FailOnRetailModeLoginSuccess();
  ExpectLoginFailure(AuthFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS));

  // Set up mock async method caller to respond as though a tmpfs mount
  // attempt has occurred and failed.
  mock_caller_->SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMountGuest(_)).Times(1).RetiresOnSaturation();

  auth_->LoginRetailMode();
  base::MessageLoop::current()->Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveDataResync) {
  UserContext expected_user_context(user_context_);
  expected_user_context.SetUserIDHash(
      cryptohome::MockAsyncMethodCaller::kFakeSanitizedUsername);
  ExpectLoginSuccess(expected_user_context);
  FailOnLoginFailure();

  // Set up mock async method caller to respond successfully to a cryptohome
  // remove attempt and a cryptohome create attempt (indicated by the
  // |CREATE_IF_MISSING| flag to AsyncMount).
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncRemove(user_context_.GetUserID(), _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_caller_,
              AsyncMount(user_context_.GetUserID(),
                         transformed_key_.GetSecret(),
                         cryptohome::CREATE_IF_MISSING,
                         _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_caller_,
              AsyncGetSanitizedUsername(user_context_.GetUserID(), _))
      .Times(1)
      .RetiresOnSaturation();

  state_->PresetOnlineLoginStatus(AuthFailure::AuthFailureNone());
  SetAttemptState(auth_.get(), state_.release());

  auth_->ResyncEncryptedData();
  base::MessageLoop::current()->Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveResyncFail) {
  FailOnLoginSuccess();
  ExpectLoginFailure(AuthFailure(AuthFailure::DATA_REMOVAL_FAILED));

  // Set up mock async method caller to fail a cryptohome remove attempt.
  mock_caller_->SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncRemove(user_context_.GetUserID(), _))
      .Times(1)
      .RetiresOnSaturation();

  SetAttemptState(auth_.get(), state_.release());

  auth_->ResyncEncryptedData();
  base::MessageLoop::current()->Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveRequestOldPassword) {
  FailOnLoginSuccess();
  ExpectPasswordChange();

  state_->PresetCryptohomeStatus(false, cryptohome::MOUNT_ERROR_KEY_FAILURE);
  state_->PresetOnlineLoginStatus(AuthFailure::AuthFailureNone());
  SetAttemptState(auth_.get(), state_.release());

  RunResolve(auth_.get());
}

TEST_F(CryptohomeAuthenticatorTest, DriveDataRecover) {
  UserContext expected_user_context(user_context_);
  expected_user_context.SetUserIDHash(
      cryptohome::MockAsyncMethodCaller::kFakeSanitizedUsername);
  ExpectLoginSuccess(expected_user_context);
  FailOnLoginFailure();

  // Set up mock async method caller to respond successfully to a key migration.
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(
      *mock_caller_,
      AsyncMigrateKey(
          user_context_.GetUserID(), _, transformed_key_.GetSecret(), _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_caller_,
              AsyncMount(user_context_.GetUserID(),
                         transformed_key_.GetSecret(),
                         cryptohome::MOUNT_FLAGS_NONE,
                         _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_caller_,
              AsyncGetSanitizedUsername(user_context_.GetUserID(), _))
      .Times(1)
      .RetiresOnSaturation();

  state_->PresetOnlineLoginStatus(AuthFailure::AuthFailureNone());
  SetAttemptState(auth_.get(), state_.release());

  auth_->RecoverEncryptedData(std::string());
  base::MessageLoop::current()->Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveDataRecoverButFail) {
  FailOnLoginSuccess();
  ExpectPasswordChange();

  // Set up mock async method caller to fail a key migration attempt,
  // asserting that the wrong password was used.
  mock_caller_->SetUp(false, cryptohome::MOUNT_ERROR_KEY_FAILURE);
  EXPECT_CALL(
      *mock_caller_,
      AsyncMigrateKey(
          user_context_.GetUserID(), _, transformed_key_.GetSecret(), _))
      .Times(1)
      .RetiresOnSaturation();

  SetAttemptState(auth_.get(), state_.release());

  auth_->RecoverEncryptedData(std::string());
  base::MessageLoop::current()->Run();
}

TEST_F(CryptohomeAuthenticatorTest, ResolveNoMountToFailedMount) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because the user doesn't exist.
  state_->PresetCryptohomeStatus(false,
                                 cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST);

  // When there is no online attempt and online results, NO_MOUNT will be
  // resolved to FAILED_MOUNT.
  EXPECT_EQ(CryptohomeAuthenticator::FAILED_MOUNT,
            SetAndResolveState(auth_.get(), state_.release()));
}

TEST_F(CryptohomeAuthenticatorTest, ResolveCreateNew) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because the user doesn't exist; additionally,
  // an online auth attempt has completed successfully.
  state_->PresetCryptohomeStatus(false,
                                 cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST);
  state_->PresetOnlineLoginStatus(AuthFailure::AuthFailureNone());

  EXPECT_EQ(CryptohomeAuthenticator::CREATE_NEW,
            SetAndResolveState(auth_.get(), state_.release()));
}

TEST_F(CryptohomeAuthenticatorTest, DriveCreateForNewUser) {
  UserContext expected_user_context(user_context_);
  expected_user_context.SetUserIDHash(
      cryptohome::MockAsyncMethodCaller::kFakeSanitizedUsername);
  ExpectLoginSuccess(expected_user_context);
  FailOnLoginFailure();

  // Set up mock async method caller to respond successfully to a cryptohome
  // create attempt (indicated by the |CREATE_IF_MISSING| flag to AsyncMount).
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_,
              AsyncMount(user_context_.GetUserID(),
                         transformed_key_.GetSecret(),
                         cryptohome::CREATE_IF_MISSING,
                         _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_caller_,
              AsyncGetSanitizedUsername(user_context_.GetUserID(), _))
      .Times(1)
      .RetiresOnSaturation();

  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because the user doesn't exist; additionally,
  // an online auth attempt has completed successfully.
  state_->PresetCryptohomeStatus(false,
                                 cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST);
  state_->PresetOnlineLoginStatus(AuthFailure::AuthFailureNone());
  SetAttemptState(auth_.get(), state_.release());

  RunResolve(auth_.get());
}

TEST_F(CryptohomeAuthenticatorTest, DriveOfflineLogin) {
  ExpectLoginSuccess(user_context_);
  FailOnLoginFailure();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  SetAttemptState(auth_.get(), state_.release());

  RunResolve(auth_.get());
}

TEST_F(CryptohomeAuthenticatorTest, DriveOnlineLogin) {
  ExpectLoginSuccess(user_context_);
  FailOnLoginFailure();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  state_->PresetOnlineLoginStatus(AuthFailure::AuthFailureNone());
  SetAttemptState(auth_.get(), state_.release());

  RunResolve(auth_.get());
}

TEST_F(CryptohomeAuthenticatorTest, DriveUnlock) {
  ExpectLoginSuccess(user_context_);
  FailOnLoginFailure();

  // Set up mock async method caller to respond successfully to a cryptohome
  // key-check attempt.
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncCheckKey(user_context_.GetUserID(), _, _))
      .Times(1)
      .RetiresOnSaturation();

  auth_->AuthenticateToUnlock(user_context_);
  base::MessageLoop::current()->Run();
}

}  // namespace chromeos
