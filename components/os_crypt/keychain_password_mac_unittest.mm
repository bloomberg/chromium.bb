// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/keychain_password_mac.h"

#include "build/build_config.h"
#include "crypto/mock_apple_keychain.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_IOS)
#include "components/os_crypt/encryption_key_creation_util_ios.h"
#else
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/os_crypt/encryption_key_creation_util_mac.h"
#include "components/os_crypt/os_crypt_features_mac.h"
#include "components/os_crypt/os_crypt_pref_names_mac.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#endif

namespace {

using crypto::MockAppleKeychain;
using os_crypt::EncryptionKeyCreationUtil;
using GetKeyAction = EncryptionKeyCreationUtil::GetKeyAction;

// An environment for KeychainPassword which initializes mock keychain with
// the given value that is going to be returned when accessing the Keychain and
// key creation utility with the given initial state (was the encryption key
// previously added to the Keychain or not and how many times overwriting the
// key was prevented initially).
class KeychainPasswordEnvironment {
 public:
  // |keychain_result| is the value that is going to be returned when accessing
  // the Keychain. If |is_key_already_created| is true, a preference that
  // indicates if the encryption key was created in the past will be set.
  // |key_overwriting_preventions| is the number of times overwriting the key
  // was prevented in a row initially.
  KeychainPasswordEnvironment(OSStatus keychain_result,
                              bool is_key_already_created,
                              int key_overwriting_preventions);

  ~KeychainPasswordEnvironment() = default;

  MockAppleKeychain& keychain() { return keychain_; }

  std::string GetPassword() const { return keychain_password_->GetPassword(); }

#if !defined(OS_IOS)
  // Returns true if the preference for key creation is set.
  bool IsKeyCreationPrefSet() const {
    return testing_local_state_.GetBoolean(os_crypt::prefs::kKeyCreated);
  }

  int KeyOverwritingPreventionsPref() const {
    return testing_local_state_.GetInteger(
        os_crypt::prefs::kKeyOverwritingPreventions);
  }
#endif

 private:
  MockAppleKeychain keychain_;
  std::unique_ptr<KeychainPassword> keychain_password_;
#if !defined(OS_IOS)
  TestingPrefServiceSimple testing_local_state_;
#endif
};

KeychainPasswordEnvironment::KeychainPasswordEnvironment(
    OSStatus keychain_find_generic_result,
    bool is_key_already_created,
    int key_overwriting_preventions) {
  // Set the value that keychain is going to return.
  keychain_.set_find_generic_result(keychain_find_generic_result);

#if !defined(OS_IOS)
  // Initialize the preference on Mac.
  testing_local_state_.registry()->RegisterBooleanPref(
      os_crypt::prefs::kKeyCreated, false);
  testing_local_state_.registry()->RegisterIntegerPref(
      os_crypt::prefs::kKeyOverwritingPreventions, 0);
  if (is_key_already_created)
    testing_local_state_.SetBoolean(os_crypt::prefs::kKeyCreated, true);
  testing_local_state_.SetInteger(os_crypt::prefs::kKeyOverwritingPreventions,
                                  key_overwriting_preventions);
#endif

// Initialize encryption key creation utility.
#if defined(OS_IOS)
  std::unique_ptr<EncryptionKeyCreationUtil> util =
      std::make_unique<os_crypt::EncryptionKeyCreationUtilIOS>();
#else
  std::unique_ptr<EncryptionKeyCreationUtil> util =
      std::make_unique<os_crypt::EncryptionKeyCreationUtilMac>(
          &testing_local_state_, base::ThreadTaskRunnerHandle::Get());
#endif

  // Initialize keychain password.
  keychain_password_ =
      std::make_unique<KeychainPassword>(keychain_, std::move(util));
}

class KeychainPasswordTest : public testing::Test {
 protected:
  KeychainPasswordTest() = default;

#if !defined(OS_IOS)
  // Waits until all tasks in the task runner's queue are finished.
  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  // If the |prevent_overwrites| is true, it enables the feature for preventing
  // encryption key overwrites. Otherwise, it disables that feature.
  void UseFeatureForPreventingKeyOverwrites(bool prevent_overwrites);

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void ExpectUniqueGetKeyAction(GetKeyAction action) {
    histogram_tester_.ExpectUniqueSample("OSCrypt.GetEncryptionKeyAction",
                                         action, 1);
  }

  void ExpectUniqueKeyOverwritingPreventions(int count) {
    histogram_tester_.ExpectUniqueSample(
        "OSCrypt.EncryptionKeyOverwritingPreventions", count, 1);
  }
#endif

 private:
#if !defined(OS_IOS)
  base::HistogramTester histogram_tester_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
#endif

  DISALLOW_COPY_AND_ASSIGN(KeychainPasswordTest);
};

#if !defined(OS_IOS)
void KeychainPasswordTest::UseFeatureForPreventingKeyOverwrites(
    bool prevent_overwrites) {
  if (prevent_overwrites) {
    scoped_feature_list_.InitAndEnableFeature(
        os_crypt::features::kPreventEncryptionKeyOverwrites);
  } else {
    scoped_feature_list_.InitAndDisableFeature(
        os_crypt::features::kPreventEncryptionKeyOverwrites);
  }
}
#endif

// Test that if we have an existing password in the Keychain and we are
// authorized by the user to read it then we get it back correctly.
TEST_F(KeychainPasswordTest, FindPasswordSuccess) {
  KeychainPasswordEnvironment environment(noErr, true, 0);
  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
}

// Test that if we do not have an existing password in the Keychain then it
// gets added successfully and returned.
TEST_F(KeychainPasswordTest, FindPasswordNotFound) {
  KeychainPasswordEnvironment environment(errSecItemNotFound, false, 0);
  EXPECT_EQ(24U, environment.GetPassword().length());
  EXPECT_TRUE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
}

// Test that if get denied access by the user then we return an empty password.
// And we should not try to add one.
TEST_F(KeychainPasswordTest, FindPasswordNotAuthorized) {
  KeychainPasswordEnvironment environment(errSecAuthFailed, false, 0);
  EXPECT_TRUE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
#if !defined(OS_IOS)
  // The key creation pref shouldn't be set.
  RunUntilIdle();
  EXPECT_FALSE(environment.IsKeyCreationPrefSet());
#endif
}

// Test that if some random other error happens then we return an empty
// password, and we should not try to add one.
TEST_F(KeychainPasswordTest, FindPasswordOtherError) {
  KeychainPasswordEnvironment environment(errSecNotAvailable, false, 0);
  EXPECT_TRUE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
#if !defined(OS_IOS)
  // The key creation pref shouldn't be set.
  RunUntilIdle();
  EXPECT_FALSE(environment.IsKeyCreationPrefSet());
#endif
}

// Test that subsequent additions to the keychain give different passwords.
TEST_F(KeychainPasswordTest, PasswordsDiffer) {
  KeychainPasswordEnvironment environment1(errSecItemNotFound, false, 0);
  std::string password1 = environment1.GetPassword();
  EXPECT_FALSE(password1.empty());
  EXPECT_TRUE(environment1.keychain().called_add_generic());
  EXPECT_EQ(0, environment1.keychain().password_data_count());

  KeychainPasswordEnvironment environment2(errSecItemNotFound, false, 0);
  std::string password2 = environment2.GetPassword();
  EXPECT_FALSE(password2.empty());
  EXPECT_TRUE(environment2.keychain().called_add_generic());
  EXPECT_EQ(0, environment2.keychain().password_data_count());

  // And finally check that the passwords are different.
  EXPECT_NE(password1, password2);
}

#if !defined(OS_IOS)
// Test that a preference is not set when the feature for preventing key
// overwrites is disabled when new key is added to the Keychain.
TEST_F(KeychainPasswordTest, PreventingOverwritesDisabledAddKey) {
  UseFeatureForPreventingKeyOverwrites(false);
  KeychainPasswordEnvironment environment(errSecItemNotFound, false, 0);
  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_TRUE(environment.keychain().called_add_generic());
  RunUntilIdle();
  EXPECT_FALSE(environment.IsKeyCreationPrefSet());
}

// Test that a key is overwritten if it was created in the past when the
// feature is disabled.
TEST_F(KeychainPasswordTest, PreventingOverwritesDisabledOverwriteKey) {
  UseFeatureForPreventingKeyOverwrites(false);
  KeychainPasswordEnvironment environment(errSecItemNotFound, true, 0);
  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_TRUE(environment.keychain().called_add_generic());
  RunUntilIdle();
  EXPECT_EQ(0, environment.KeyOverwritingPreventionsPref());
}

// Test that a new key is not added if one should already exist in the Keychain,
// and that an empty string is returned.
TEST_F(KeychainPasswordTest, PreventingOverwritesEnabledKeyExistsButNotFound) {
  UseFeatureForPreventingKeyOverwrites(true);
  KeychainPasswordEnvironment environment(errSecItemNotFound, true, 0);

  EXPECT_TRUE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  RunUntilIdle();
  // Make sure that prevention counter has increased.
  EXPECT_EQ(1, environment.KeyOverwritingPreventionsPref());

  ExpectUniqueGetKeyAction(GetKeyAction::kOverwritingPrevented);
  ExpectUniqueKeyOverwritingPreventions(1);
}

// Test that a new key is added if one doesn't already exist in the Keychain,
// and that the key creation preference is set.
TEST_F(KeychainPasswordTest, PreventingOverwritesEnabledAddNewKey) {
  UseFeatureForPreventingKeyOverwrites(true);
  KeychainPasswordEnvironment environment(errSecItemNotFound, false, 0);

  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_TRUE(environment.keychain().called_add_generic());
  RunUntilIdle();
  EXPECT_TRUE(environment.IsKeyCreationPrefSet());
  EXPECT_EQ(0, environment.KeyOverwritingPreventionsPref());

  ExpectUniqueGetKeyAction(GetKeyAction::kNewKeyAddedToKeychain);
  ExpectUniqueKeyOverwritingPreventions(0);
}

// Test that the key creation preference is set when successfully accessing the
// key from the Keychain for the first time.
TEST_F(KeychainPasswordTest, PreventingOverwritesEnabledFindKeyTheFirstTime) {
  UseFeatureForPreventingKeyOverwrites(true);
  KeychainPasswordEnvironment environment(noErr, false, 0);

  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  RunUntilIdle();
  EXPECT_TRUE(environment.IsKeyCreationPrefSet());
  EXPECT_EQ(0, environment.KeyOverwritingPreventionsPref());

  ExpectUniqueGetKeyAction(GetKeyAction::kKeyFoundFirstTime);
  ExpectUniqueKeyOverwritingPreventions(0);
}

// Test that the key creation preference is not set, that an empty password is
// returned and no password is added to the Keychain if an error other than
// errSecItemNotFound is returned by the Keychain.
TEST_F(KeychainPasswordTest, PreventingOverwritesEnabledOtherError) {
  UseFeatureForPreventingKeyOverwrites(true);
  KeychainPasswordEnvironment environment(errSecNotAvailable, false, 0);

  EXPECT_TRUE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  RunUntilIdle();
  EXPECT_FALSE(environment.IsKeyCreationPrefSet());
  EXPECT_EQ(0, environment.KeyOverwritingPreventionsPref());

  ExpectUniqueGetKeyAction(GetKeyAction::kKeychainLookupFailed);
  EXPECT_TRUE(histogram_tester()
                  .GetAllSamples("OSCrypt.EncryptionKeyOverwritingPreventions")
                  .empty());
}

TEST_F(KeychainPasswordTest, PreventingOverwritesEnabledKeyFoundSecondTime) {
  UseFeatureForPreventingKeyOverwrites(true);
  KeychainPasswordEnvironment environment(noErr, true, 1);

  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  RunUntilIdle();
  EXPECT_EQ(0, environment.KeyOverwritingPreventionsPref());

  ExpectUniqueGetKeyAction(GetKeyAction::kKeyFound);
  ExpectUniqueKeyOverwritingPreventions(0);
}
#endif  // !defined(OS_IOS)

}  // namespace
