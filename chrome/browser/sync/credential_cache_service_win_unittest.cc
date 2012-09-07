// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/sync/credential_cache_service_win.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class CredentialCacheServiceTest : public CredentialCacheService,
                                   public testing::Test {
 public:
  CredentialCacheServiceTest()
      : CredentialCacheService(NULL),
        file_message_loop_(MessageLoop::TYPE_IO) {}

  virtual ~CredentialCacheServiceTest() {}

  // testing::Test implementation.
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    set_local_store(new JsonPrefStore(
        temp_dir_.path().Append(chrome::kSyncCredentialsFilename),
        file_message_loop_.message_loop_proxy()));
  }

  // testing::Test implementation.
  virtual void TearDown() OVERRIDE {
    file_message_loop_.RunAllPending();
  }

 private:
  ScopedTempDir temp_dir_;
  MessageLoop file_message_loop_;
  DISALLOW_COPY_AND_ASSIGN(CredentialCacheServiceTest);
};

TEST_F(CredentialCacheServiceTest, TestPackAndUnpack) {
  // Pack a sample credential string.
  std::string original = "sample_credential";
  scoped_ptr<base::StringValue> packed(
      syncer::CredentialCacheService::PackCredential(original));

  // Unpack the result and make sure it matches the original.
  std::string unpacked =
      syncer::CredentialCacheService::UnpackCredential(*packed);
  ASSERT_EQ(original, unpacked);
}

TEST_F(CredentialCacheServiceTest, TestPackAndUnpackEmpty) {
  // Pack an empty credential string.
  std::string original = "";
  scoped_ptr<base::StringValue> packed(
      syncer::CredentialCacheService::PackCredential(original));

  // Make sure the packed value is an empty string.
  std::string packed_string;
  packed->GetAsString(&packed_string);
  ASSERT_EQ(original, packed_string);

  // Unpack it and make sure it matches the original.
  std::string unpacked =
      syncer::CredentialCacheService::UnpackCredential(*packed);
  ASSERT_EQ(original, unpacked);
}

TEST_F(CredentialCacheServiceTest, TestWriteAndReadCredentials) {
  std::string username = "user@gmail.com";
  std::string lsid = "lsid";
  bool sync_everything = true;

  // Write a string pref, a token and a boolean pref.
  PackAndUpdateStringPref(prefs::kGoogleServicesUsername, username);
  PackAndUpdateStringPref(GaiaConstants::kGaiaLsid, lsid);
  UpdateBooleanPref(prefs::kSyncKeepEverythingSynced, sync_everything);

  // Verify that they can be read, and that they contain the original values.
  ASSERT_TRUE(HasPref(local_store(), prefs::kGoogleServicesUsername));
  ASSERT_EQ(username, GetAndUnpackStringPref(local_store(),
                                             prefs::kGoogleServicesUsername));
  ASSERT_TRUE(HasPref(local_store(), GaiaConstants::kGaiaLsid));
  ASSERT_EQ(lsid, GetAndUnpackStringPref(local_store(),
                                         GaiaConstants::kGaiaLsid));
  ASSERT_TRUE(HasPref(local_store(), prefs::kSyncKeepEverythingSynced));
  ASSERT_TRUE(sync_everything ==
              GetBooleanPref(local_store(),
                             prefs::kSyncKeepEverythingSynced));
}

TEST_F(CredentialCacheServiceTest, TestWriteAndReadCredentialsAfterSignOut) {
  std::string username = "user@gmail.com";
  std::string lsid = "lsid";
  bool sync_everything = true;

  // Write a non-empty username, indicating that the user is signed in.
  PackAndUpdateStringPref(prefs::kGoogleServicesUsername, username);
  ASSERT_TRUE(HasPref(local_store(), prefs::kGoogleServicesUsername));
  ASSERT_EQ(username, GetAndUnpackStringPref(local_store(),
                                             prefs::kGoogleServicesUsername));

  // Write an empty username, indicating that the user is signed out.
  PackAndUpdateStringPref(prefs::kGoogleServicesUsername, "");
  ASSERT_TRUE(HasPref(local_store(), prefs::kGoogleServicesUsername));
  ASSERT_EQ(std::string(),
            GetAndUnpackStringPref(local_store(),
                                   prefs::kGoogleServicesUsername));

  // Try to write a non-empty string and make sure an empty string is written in
  // its place.
  PackAndUpdateStringPref(GaiaConstants::kGaiaLsid, lsid);
  ASSERT_TRUE(HasPref(local_store(), GaiaConstants::kGaiaLsid));
  ASSERT_EQ(std::string(), GetAndUnpackStringPref(local_store(),
                                                  GaiaConstants::kGaiaLsid));

  // Try to write a boolean pref with value true, and make sure an the default
  // value of false is written in its place.
  UpdateBooleanPref(prefs::kSyncKeepEverythingSynced, sync_everything);
  ASSERT_TRUE(HasPref(local_store(), prefs::kSyncKeepEverythingSynced));
  ASSERT_TRUE(false == GetBooleanPref(local_store(),
                                      prefs::kSyncKeepEverythingSynced));
}

TEST_F(CredentialCacheServiceTest, TestWriteAndReadTimestamps) {
  // Write a timestamp.
  WriteLastCacheUpdateTime();

  // Make sure that the correct field was written, and it is not in the future.
  base::Time time(GetLastCacheUpdateTime(local_store()));
  ASSERT_LE(time, base::Time::Now());
}

}  // namespace syncer
