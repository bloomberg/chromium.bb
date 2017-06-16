// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/hash_password_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

class HashPasswordManagerTest : public testing::Test {
 public:
  // testing::Test:
  void SetUp() override;

 protected:
  TestingPrefServiceSimple prefs_;
};

void HashPasswordManagerTest::SetUp() {
  prefs_.registry()->RegisterStringPref(prefs::kSyncPasswordHash, std::string(),
                                        PrefRegistry::NO_REGISTRATION_FLAGS);
}

TEST_F(HashPasswordManagerTest, Saving) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));
  HashPasswordManager hash_password_manager(&prefs_);
  hash_password_manager.SavePasswordHash(base::ASCIIToUTF16("sync_password"));
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));
}

TEST_F(HashPasswordManagerTest, Clearing) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));
  HashPasswordManager hash_password_manager(&prefs_);
  hash_password_manager.SavePasswordHash(base::ASCIIToUTF16("sync_password"));
  hash_password_manager.ClearSavedPasswordHash();
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));
}

TEST_F(HashPasswordManagerTest, Retrieving) {
  ASSERT_FALSE(prefs_.HasPrefPath(prefs::kSyncPasswordHash));
  HashPasswordManager hash_password_manager(&prefs_);
  hash_password_manager.SavePasswordHash(base::ASCIIToUTF16("sync_password"));
  // TODO(crbug.com/657041) Fix this text when hash calculation is implemented.
  EXPECT_FALSE(hash_password_manager.RetrievePasswordHash());
}

}  // namespace
}  // namespace password_manager
