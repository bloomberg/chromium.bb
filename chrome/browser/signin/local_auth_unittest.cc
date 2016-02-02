// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/local_auth.h"

#include <stddef.h>

#include "base/base64.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/os_crypt/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"

#include "testing/gtest/include/gtest/gtest.h"

class LocalAuthTest : public testing::Test {
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(LocalAuthTest, SetAndCheckCredentials) {
  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  Profile* prof = testing_profile_manager.CreateTestingProfile("p1");
  ProfileInfoCache& cache =
      testing_profile_manager.profile_manager()->GetProfileInfoCache();
  EXPECT_EQ(1U, cache.GetNumberOfProfiles());
  EXPECT_EQ("", cache.GetLocalAuthCredentialsOfProfileAtIndex(0));

#if defined(OS_MACOSX)
  OSCrypt::UseMockKeychain(true);
#endif

  std::string password("Some Password");
  EXPECT_FALSE(LocalAuth::ValidateLocalAuthCredentials(prof, password));

  LocalAuth::SetLocalAuthCredentials(prof, password);
  std::string passhash = cache.GetLocalAuthCredentialsOfProfileAtIndex(0);

  // We perform basic validation on the written record to ensure bugs don't slip
  // in that cannot be seen from the API:
  //  - The encoding exists (we can guarantee future backward compatibility).
  //  - The plaintext version of the password is not mistakenly stored anywhere.
  EXPECT_FALSE(passhash.empty());
  EXPECT_EQ('2', passhash[0]);
  EXPECT_EQ(passhash.find(password), std::string::npos);

  std::string decodedhash;
  base::Base64Decode(passhash.substr(1), &decodedhash);
  EXPECT_FALSE(decodedhash.empty());
  EXPECT_EQ(decodedhash.find(password), std::string::npos);

  EXPECT_TRUE(LocalAuth::ValidateLocalAuthCredentials(prof, password));
  EXPECT_FALSE(LocalAuth::ValidateLocalAuthCredentials(prof, password + "1"));

  LocalAuth::SetLocalAuthCredentials(prof, password);  // makes different salt
  EXPECT_NE(passhash, cache.GetLocalAuthCredentialsOfProfileAtIndex(0));
}

TEST_F(LocalAuthTest, SetUpgradeAndCheckCredentials) {
  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  Profile* prof = testing_profile_manager.CreateTestingProfile("p1");
  ProfileInfoCache& cache =
      testing_profile_manager.profile_manager()->GetProfileInfoCache();

#if defined(OS_MACOSX)
  OSCrypt::UseMockKeychain(true);
#endif

  std::string password("Some Password");
  size_t profile_index = cache.GetIndexOfProfileWithPath(prof->GetPath());
  LocalAuth::SetLocalAuthCredentialsWithEncoding(profile_index, password, '1');

  // Ensure we indeed persisted the correct encoding.
  std::string oldpasshash = cache.GetLocalAuthCredentialsOfProfileAtIndex(
      profile_index);
  EXPECT_EQ('1', oldpasshash[0]);

  // Validate, ensure we can validate against the old encoding.
  EXPECT_TRUE(LocalAuth::ValidateLocalAuthCredentials(prof, password));

  // Ensure we updated the encoding.
  std::string newpasshash = cache.GetLocalAuthCredentialsOfProfileAtIndex(
      profile_index);
  EXPECT_EQ('2', newpasshash[0]);
  // Encoding '2' writes fewer bytes than encoding '1'.
  EXPECT_LE(newpasshash.length(), oldpasshash.length());

  // Validate, ensure we validate against the new encoding.
  EXPECT_TRUE(LocalAuth::ValidateLocalAuthCredentials(prof, password));
}

// Test truncation where each byte is left whole.
TEST_F(LocalAuthTest, TruncateStringEvenly) {
  std::string two_chars = "A6";
  std::string three_chars = "A6C";
  EXPECT_EQ(two_chars, LocalAuth::TruncateStringByBits(two_chars, 16));
  EXPECT_EQ(two_chars, LocalAuth::TruncateStringByBits(three_chars, 16));

  EXPECT_EQ(two_chars, LocalAuth::TruncateStringByBits(two_chars, 14));
  EXPECT_EQ(two_chars, LocalAuth::TruncateStringByBits(three_chars, 14));
}

// Test truncation that affects the results within a byte.
TEST_F(LocalAuthTest, TruncateStringUnevenly) {
  std::string two_chars = "Az";
  std::string three_chars = "AzC";
  // 'z' = 0x7A, ':' = 0x3A.
  std::string two_chars_truncated = "A:";
  EXPECT_EQ(two_chars_truncated,
      LocalAuth::TruncateStringByBits(two_chars, 14));
  EXPECT_EQ(two_chars_truncated,
      LocalAuth::TruncateStringByBits(three_chars, 14));
}
