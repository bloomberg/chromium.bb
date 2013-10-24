// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/local_auth.h"

#include "base/base64.h"
#include "base/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webdata/encryptor/encryptor.h"

#include "testing/gtest/include/gtest/gtest.h"

using namespace chrome;

class LocalAuthTest : public testing::Test {
 public:
  LocalAuthTest() {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
  }

  scoped_ptr<TestingProfile> profile_;
};

TEST_F(LocalAuthTest, SetAndCheckCredentials) {
  Profile* prof = profile_.get();
  PrefService* prefs = prof->GetPrefs();
  EXPECT_FALSE(prefs->HasPrefPath(prefs::kGoogleServicesPasswordHash));

#if defined(OS_MACOSX)
    Encryptor::UseMockKeychain(true);
#endif

  std::string username("user@gmail.com");
  std::string password("Some Password");
  EXPECT_FALSE(ValidateLocalAuthCredentials(prof, username, password));

  SetLocalAuthCredentials(prof, username, password);
  std::string passhash = prefs->GetString(prefs::kGoogleServicesPasswordHash);

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
  EXPECT_EQ(decodedhash.find(username), std::string::npos);
  EXPECT_EQ(decodedhash.find(password), std::string::npos);

  EXPECT_TRUE(ValidateLocalAuthCredentials(prof, username, password));
  EXPECT_FALSE(ValidateLocalAuthCredentials(prof, username + "1", password));
  EXPECT_FALSE(ValidateLocalAuthCredentials(prof, username, password + "1"));

  SetLocalAuthCredentials(prof, username, password);  // makes different salt
  EXPECT_NE(prefs->GetString(prefs::kGoogleServicesPasswordHash), passhash);
}
