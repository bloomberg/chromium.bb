// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/hash_password_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

class HashPasswordManagerTest : public testing::Test {
 public:
  HashPasswordManagerTest() {
    prefs_.registry()->RegisterStringPref(prefs::kSyncPasswordHash,
                                          std::string(),
                                          PrefRegistry::NO_REGISTRATION_FLAGS);
    prefs_.registry()->RegisterStringPref(prefs::kSyncPasswordLengthAndHashSalt,
                                          std::string(),
                                          PrefRegistry::NO_REGISTRATION_FLAGS);
    // Mock OSCrypt. There is a call to OSCrypt on initializling
    // PasswordReuseDetector, so it should be mocked.
    OSCryptMocker::SetUp();
  }

  ~HashPasswordManagerTest() override { OSCryptMocker::TearDown(); }

 protected:
  TestingPrefServiceSimple prefs_;
};

TEST_F(HashPasswordManagerTest, Saving) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  hash_password_manager.SavePasswordHash(base::ASCIIToUTF16("sync_password"));
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));

  // Saves the same password again won't change password hash, length or salt.
  const std::string current_hash = prefs_.GetString(prefs::kSyncPasswordHash);
  const std::string current_length_and_salt =
      prefs_.GetString(prefs::kSyncPasswordLengthAndHashSalt);
  hash_password_manager.SavePasswordHash(base::ASCIIToUTF16("sync_password"));
  EXPECT_EQ(current_hash, prefs_.GetString(prefs::kSyncPasswordHash));
  EXPECT_EQ(current_length_and_salt,
            prefs_.GetString(prefs::kSyncPasswordLengthAndHashSalt));
}

TEST_F(HashPasswordManagerTest, Clearing) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  hash_password_manager.SavePasswordHash(base::ASCIIToUTF16("sync_password"));
  hash_password_manager.ClearSavedPasswordHash();
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));
}

TEST_F(HashPasswordManagerTest, Retrieving) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(&prefs_);
  hash_password_manager.SavePasswordHash(base::ASCIIToUTF16("sync_password"));
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kSyncPasswordLengthAndHashSalt));

  base::Optional<SyncPasswordData> sync_password_data =
      hash_password_manager.RetrievePasswordHash();
  ASSERT_TRUE(sync_password_data);
  EXPECT_EQ(13u, sync_password_data->length);
  EXPECT_EQ(16u, sync_password_data->salt.size());
  uint64_t expected_hash = password_manager_util::CalculateSyncPasswordHash(
      base::ASCIIToUTF16("sync_password"), sync_password_data->salt);
  EXPECT_EQ(expected_hash, sync_password_data->hash);
}

}  // namespace
}  // namespace password_manager
