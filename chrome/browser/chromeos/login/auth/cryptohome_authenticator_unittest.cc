// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/login/auth/chrome_cryptohome_authenticator.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/cryptohome/mock_homedir_methods.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/mock_auth_status_consumer.h"
#include "chromeos/login/auth/mock_url_fetchers.h"
#include "chromeos/login/auth/test_attempt_state.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/login/login_state.h"
#include "components/ownership/mock_owner_key_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "crypto/nss_key_util.h"
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
using ::testing::WithArg;
using ::testing::_;

namespace chromeos {

namespace {

// Label under which the user's key is stored.
const char kCryptohomeGAIAKeyLabel[] = "gaia";

// Salt used by pre-hashed key.
const char kSalt[] = "SALT $$";

// An owner key in PKCS#8 PrivateKeyInfo for testing owner checks.
const uint8_t kOwnerPrivateKey[] = {
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
const uint8_t kOwnerPublicKey[] = {
    0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
    0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x4b, 0x00, 0x30, 0x48, 0x02, 0x41,
    0x00, 0xb4, 0xf5, 0xab, 0xfe, 0xd8, 0xf1, 0xcb, 0x5f, 0x8f, 0x48, 0x3e,
    0xdf, 0x40, 0x8e, 0x2b, 0x15, 0x43, 0x6c, 0x67, 0x74, 0xa2, 0xcb, 0xe4,
    0xf3, 0xec, 0xab, 0x41, 0x57, 0x1d, 0x5f, 0xed, 0xcf, 0x09, 0xf4, 0xcc,
    0xbb, 0x52, 0x52, 0xe8, 0x46, 0xf5, 0xc5, 0x01, 0xa3, 0xd8, 0x24, 0xc0,
    0x15, 0xc5, 0x65, 0x50, 0x7d, 0xbd, 0x4e, 0x81, 0xb2, 0x28, 0x38, 0xf9,
    0x3d, 0x3e, 0x2a, 0x68, 0xf7, 0x02, 0x03, 0x01, 0x00, 0x01};

std::vector<uint8_t> GetOwnerPublicKey() {
  return std::vector<uint8_t>(kOwnerPublicKey,
                              kOwnerPublicKey + arraysize(kOwnerPublicKey));
}

bool CreateOwnerKeyInSlot(PK11SlotInfo* slot) {
  const std::vector<uint8_t> key(
      kOwnerPrivateKey, kOwnerPrivateKey + arraysize(kOwnerPrivateKey));
  return crypto::ImportNSSKeyFromPrivateKeyInfo(
             slot, key, true /* permanent */) != nullptr;
}

}  // namespace

class CryptohomeAuthenticatorTest : public testing::Test {
 public:
  CryptohomeAuthenticatorTest()
      : user_context_(AccountId::FromUserEmail("me@nowhere.org")),
        user_manager_(new chromeos::FakeChromeUserManager()),
        user_manager_enabler_(user_manager_),
        mock_caller_(NULL),
        mock_homedir_methods_(NULL),
        owner_key_util_(new ownership::MockOwnerKeyUtil()) {
    OwnerSettingsServiceChromeOSFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
    user_context_.SetKey(Key("fakepass"));
    user_context_.SetUserIDHash("me_nowhere_com_hash");
    const user_manager::User* user =
        user_manager_->AddUser(user_context_.GetAccountId());
    profile_.set_profile_name(user_context_.GetAccountId().GetUserEmail());

    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, &profile_);

    CreateTransformedKey(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                         SystemSaltGetter::ConvertRawSaltToHexString(
                             FakeCryptohomeClient::GetStubSystemSalt()));
  }

  ~CryptohomeAuthenticatorTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kLoginManager);

    mock_caller_ = new cryptohome::MockAsyncMethodCaller;
    cryptohome::AsyncMethodCaller::InitializeForTesting(mock_caller_);
    mock_homedir_methods_ = new cryptohome::MockHomedirMethods;
    mock_homedir_methods_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
    cryptohome::HomedirMethods::InitializeForTesting(mock_homedir_methods_);

    fake_cryptohome_client_ = new FakeCryptohomeClient;
    chromeos::DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
        std::unique_ptr<CryptohomeClient>(fake_cryptohome_client_));

    SystemSaltGetter::Initialize();

    auth_ = new ChromeCryptohomeAuthenticator(&consumer_);
    state_.reset(new TestAttemptState(user_context_, false));
  }

  // Tears down the test fixture.
  void TearDown() override {
    SystemSaltGetter::Shutdown();
    DBusThreadManager::Shutdown();

    cryptohome::AsyncMethodCaller::Shutdown();
    mock_caller_ = NULL;
    cryptohome::HomedirMethods::Shutdown();
    mock_homedir_methods_ = NULL;
  }

  void CreateTransformedKey(Key::KeyType type, const std::string& salt) {
    user_context_with_transformed_key_ = user_context_;
    user_context_with_transformed_key_.GetKey()->Transform(type, salt);
    transformed_key_ = *user_context_with_transformed_key_.GetKey();
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

  void ExpectGetKeyDataExCall(std::unique_ptr<int64_t> key_type,
                              std::unique_ptr<std::string> salt) {
    key_definitions_.clear();
    key_definitions_.push_back(cryptohome::KeyDefinition(
        std::string() /* secret */,
        kCryptohomeGAIAKeyLabel,
        cryptohome::PRIV_DEFAULT));
    cryptohome::KeyDefinition& key_definition = key_definitions_.back();
    key_definition.revision = 1;
    if (key_type) {
      key_definition.provider_data.push_back(
          cryptohome::KeyDefinition::ProviderData("type"));
      key_definition.provider_data.back().number = std::move(key_type);
    }
    if (salt) {
      key_definition.provider_data.push_back(
          cryptohome::KeyDefinition::ProviderData("salt"));
      key_definition.provider_data.back().bytes = std::move(salt);
    }
    EXPECT_CALL(
        *mock_homedir_methods_,
        GetKeyDataEx(cryptohome::Identification(user_context_.GetAccountId()),
                     kCryptohomeGAIAKeyLabel, _))
        .WillOnce(WithArg<2>(Invoke(
            this, &CryptohomeAuthenticatorTest::InvokeGetDataExCallback)));
  }

  void ExpectMountExCall(bool expect_create_attempt) {
    const cryptohome::KeyDefinition auth_key(transformed_key_.GetSecret(),
                                             std::string(),
                                             cryptohome::PRIV_DEFAULT);
    cryptohome::MountParameters mount(false /* ephemeral */);
    if (expect_create_attempt) {
      mount.create_keys.push_back(cryptohome::KeyDefinition(
          transformed_key_.GetSecret(),
          kCryptohomeGAIAKeyLabel,
          cryptohome::PRIV_DEFAULT));
    }
    EXPECT_CALL(
        *mock_homedir_methods_,
        MountEx(cryptohome::Identification(user_context_.GetAccountId()),
                cryptohome::Authorization(auth_key), mount, _))
        .Times(1)
        .RetiresOnSaturation();
  }

  void RunResolve(CryptohomeAuthenticator* auth) {
    auth->Resolve();
    base::RunLoop().RunUntilIdle();
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
  UserContext user_context_with_transformed_key_;
  Key transformed_key_;

  std::vector<cryptohome::KeyDefinition> key_definitions_;

  ScopedDeviceSettingsTestHelper device_settings_test_helper_;
  ScopedTestCrosSettings test_cros_settings_;

  TestingProfile profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  chromeos::FakeChromeUserManager* user_manager_;
  ScopedUserManagerEnabler user_manager_enabler_;

  cryptohome::MockAsyncMethodCaller* mock_caller_;
  cryptohome::MockHomedirMethods* mock_homedir_methods_;

  MockAuthStatusConsumer consumer_;

  scoped_refptr<CryptohomeAuthenticator> auth_;
  std::unique_ptr<TestAttemptState> state_;
  FakeCryptohomeClient* fake_cryptohome_client_;

  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;

 private:
  void InvokeGetDataExCallback(
      const cryptohome::HomedirMethods::GetKeyDataCallback& callback) {
    callback.Run(true /* success */,
                 cryptohome::MOUNT_ERROR_NONE,
                 key_definitions_);
  }
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
  ScopedCrosSettingsTestHelper settings_helper(false);
  settings_helper.ReplaceProvider(kPolicyMissingMitigationMode);
  settings_helper.SetBoolean(kPolicyMissingMitigationMode, true);

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
}

// Test the case that login switches to SafeMode and the Owner logs in, which
// should lead to a successful login.
TEST_F(CryptohomeAuthenticatorTest, ResolveOwnerNeededSuccess) {
  crypto::ScopedTestNSSChromeOSUser test_user_db(user_context_.GetUserIDHash());
  owner_key_util_->SetPublicKey(GetOwnerPublicKey());

  crypto::ScopedPK11Slot user_slot(
      crypto::GetPublicSlotForChromeOSUser(user_context_.GetUserIDHash()));
  ASSERT_TRUE(CreateOwnerKeyInSlot(user_slot.get()));

  profile_manager_.reset(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  ASSERT_TRUE(profile_manager_->SetUp());

  ExpectLoginSuccess(user_context_);

  // Set up state as though a cryptohome mount attempt has occurred
  // and succeeded but we are in safe mode and the current user is not owner.
  state_->PresetCryptohomeStatus(true, cryptohome::MOUNT_ERROR_NONE);
  SetOwnerState(false, false);
  ScopedCrosSettingsTestHelper settings_helper(false);
  settings_helper.ReplaceProvider(kPolicyMissingMitigationMode);
  settings_helper.SetBoolean(kPolicyMissingMitigationMode, true);

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
  base::RunLoop().Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveGuestLoginButFail) {
  FailOnGuestLoginSuccess();
  ExpectLoginFailure(AuthFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS));

  // Set up mock async method caller to respond as though a tmpfs mount
  // attempt has occurred and failed.
  mock_caller_->SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(*mock_caller_, AsyncMountGuest(_)).Times(1).RetiresOnSaturation();

  auth_->LoginOffTheRecord();
  base::RunLoop().Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveDataResync) {
  UserContext expected_user_context(user_context_with_transformed_key_);
  expected_user_context.SetUserIDHash(
      cryptohome::MockAsyncMethodCaller::kFakeSanitizedUsername);
  ExpectLoginSuccess(expected_user_context);
  FailOnLoginFailure();

  // Set up mock async method caller to respond successfully to a cryptohome
  // remove attempt.
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(
      *mock_caller_,
      AsyncRemove(cryptohome::Identification(user_context_.GetAccountId()), _))
      .Times(1)
      .RetiresOnSaturation();

  // Set up mock homedir methods to respond successfully to a cryptohome create
  // attempt.
  ExpectGetKeyDataExCall(std::unique_ptr<int64_t>(),
                         std::unique_ptr<std::string>());
  ExpectMountExCall(true /* expect_create_attempt */);

  state_->PresetOnlineLoginStatus(AuthFailure::AuthFailureNone());
  SetAttemptState(auth_.get(), state_.release());

  auth_->ResyncEncryptedData();
  base::RunLoop().Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveResyncFail) {
  FailOnLoginSuccess();
  ExpectLoginFailure(AuthFailure(AuthFailure::DATA_REMOVAL_FAILED));

  // Set up mock async method caller to fail a cryptohome remove attempt.
  mock_caller_->SetUp(false, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(
      *mock_caller_,
      AsyncRemove(cryptohome::Identification(user_context_.GetAccountId()), _))
      .Times(1)
      .RetiresOnSaturation();

  SetAttemptState(auth_.get(), state_.release());

  auth_->ResyncEncryptedData();
  base::RunLoop().Run();
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
  UserContext expected_user_context(user_context_with_transformed_key_);
  expected_user_context.SetUserIDHash(
      cryptohome::MockAsyncMethodCaller::kFakeSanitizedUsername);
  ExpectLoginSuccess(expected_user_context);
  FailOnLoginFailure();

  // Set up mock async method caller to respond successfully to a key migration.
  mock_caller_->SetUp(true, cryptohome::MOUNT_ERROR_NONE);
  EXPECT_CALL(
      *mock_caller_,
      AsyncMigrateKey(cryptohome::Identification(user_context_.GetAccountId()),
                      _, transformed_key_.GetSecret(), _))
      .Times(1)
      .RetiresOnSaturation();

  // Set up mock homedir methods to respond successfully to a cryptohome mount
  // attempt.
  ExpectGetKeyDataExCall(std::unique_ptr<int64_t>(),
                         std::unique_ptr<std::string>());
  ExpectMountExCall(false /* expect_create_attempt */);

  state_->PresetOnlineLoginStatus(AuthFailure::AuthFailureNone());
  SetAttemptState(auth_.get(), state_.release());

  auth_->RecoverEncryptedData(std::string());
  base::RunLoop().Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveDataRecoverButFail) {
  FailOnLoginSuccess();
  ExpectPasswordChange();

  // Set up mock async method caller to fail a key migration attempt,
  // asserting that the wrong password was used.
  mock_caller_->SetUp(false, cryptohome::MOUNT_ERROR_KEY_FAILURE);
  EXPECT_CALL(
      *mock_caller_,
      AsyncMigrateKey(cryptohome::Identification(user_context_.GetAccountId()),
                      _, transformed_key_.GetSecret(), _))
      .Times(1)
      .RetiresOnSaturation();

  SetAttemptState(auth_.get(), state_.release());

  auth_->RecoverEncryptedData(std::string());
  base::RunLoop().Run();
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
  UserContext expected_user_context(user_context_with_transformed_key_);
  expected_user_context.SetUserIDHash(
      cryptohome::MockAsyncMethodCaller::kFakeSanitizedUsername);
  ExpectLoginSuccess(expected_user_context);
  FailOnLoginFailure();

  // Set up mock homedir methods to respond successfully to a cryptohome create
  // attempt.
  ExpectGetKeyDataExCall(std::unique_ptr<int64_t>(),
                         std::unique_ptr<std::string>());
  ExpectMountExCall(true /* expect_create_attempt */);

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
  EXPECT_CALL(
      *mock_caller_,
      AsyncCheckKey(cryptohome::Identification(user_context_.GetAccountId()), _,
                    _))
      .Times(1)
      .RetiresOnSaturation();

  auth_->AuthenticateToUnlock(user_context_);
  base::RunLoop().Run();
}

TEST_F(CryptohomeAuthenticatorTest, DriveLoginWithPreHashedPassword) {
  CreateTransformedKey(Key::KEY_TYPE_SALTED_SHA256, kSalt);

  UserContext expected_user_context(user_context_with_transformed_key_);
  expected_user_context.SetUserIDHash(
      cryptohome::MockAsyncMethodCaller::kFakeSanitizedUsername);
  ExpectLoginSuccess(expected_user_context);
  FailOnLoginFailure();

  // Set up mock homedir methods to respond with key metadata indicating that a
  // pre-hashed key was used to create the cryptohome and allow a successful
  // mount when this pre-hashed key is used.

  ExpectGetKeyDataExCall(base::MakeUnique<int64_t>(Key::KEY_TYPE_SALTED_SHA256),
                         base::MakeUnique<std::string>(kSalt));
  ExpectMountExCall(false /* expect_create_attempt */);

  auth_->AuthenticateToLogin(NULL, user_context_);
  base::RunLoop().Run();
}

TEST_F(CryptohomeAuthenticatorTest, FailLoginWithMissingSalt) {
  CreateTransformedKey(Key::KEY_TYPE_SALTED_SHA256, kSalt);

  FailOnLoginSuccess();
  ExpectLoginFailure(AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME));

  // Set up mock homedir methods to respond with key metadata indicating that a
  // pre-hashed key was used to create the cryptohome but without the required
  // salt.
  ExpectGetKeyDataExCall(base::MakeUnique<int64_t>(Key::KEY_TYPE_SALTED_SHA256),
                         std::unique_ptr<std::string>());

  auth_->AuthenticateToLogin(NULL, user_context_);
  base::RunLoop().Run();
}

}  // namespace chromeos
