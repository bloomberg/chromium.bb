// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/local_auth.h"

#include "base/base64.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/os_crypt/os_crypt.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace chrome;

TEST(LocalAuthTest, SetAndCheckCredentials) {
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
  EXPECT_FALSE(ValidateLocalAuthCredentials(prof, password));

  SetLocalAuthCredentials(prof, password);
  std::string passhash = cache.GetLocalAuthCredentialsOfProfileAtIndex(0);

  // We perform basic validation on the written record to ensure bugs don't slip
  // in that cannot be seen from the API:
  //  - The encoding exists (we can guarantee future backward compatibility).
  //  - The plaintext version of the password is not mistakenly stored anywhere.
  EXPECT_FALSE(passhash.empty());
  EXPECT_EQ('1', passhash[0]);
  EXPECT_EQ(passhash.find(password), std::string::npos);

  std::string decodedhash;
  base::Base64Decode(passhash.substr(1), &decodedhash);
  EXPECT_FALSE(decodedhash.empty());
  EXPECT_EQ(decodedhash.find(password), std::string::npos);

  EXPECT_TRUE(ValidateLocalAuthCredentials(prof, password));
  EXPECT_FALSE(ValidateLocalAuthCredentials(prof, password + "1"));

  SetLocalAuthCredentials(prof, password);  // makes different salt
  EXPECT_NE(passhash, cache.GetLocalAuthCredentialsOfProfileAtIndex(0));
}
