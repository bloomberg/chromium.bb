// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace util {
namespace {

TEST(FileManagerPathUtilTest, MultiProfileDownloadsFolderMigration) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;

  // This looks like "/home/chronos/u-hash/Downloads" in the production
  // environment.
  const base::FilePath kDownloads = GetDownloadsFolderForProfile(&profile);
  const base::FilePath kOldDownloads =
      DownloadPrefs::GetDefaultDownloadDirectory();

  base::FilePath path;

  EXPECT_TRUE(MigratePathFromOldFormat(&profile, kOldDownloads, &path));
  EXPECT_EQ(kDownloads, path);

  EXPECT_TRUE(MigratePathFromOldFormat(
      &profile,
      kOldDownloads.AppendASCII("a/b"),
      &path));
  EXPECT_EQ(kDownloads.AppendASCII("a/b"), path);

  // Path already in the new format is not converted.
  EXPECT_FALSE(MigratePathFromOldFormat(
      &profile,
      kDownloads.AppendASCII("a/b"),
      &path));

  // Only the "Downloads" path is converted.
  EXPECT_FALSE(MigratePathFromOldFormat(
      &profile,
      base::FilePath::FromUTF8Unsafe("/home/chronos/user/dl"),
      &path));
}

TEST(FileManagerPathUtilTest, ConvertPathToArcUrl) {
  content::TestBrowserThreadBundle thread_bundle;

  // Test SetUp -- add two user-profile pairs and their fake managers.
  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());

  chromeos::FakeChromeUserManager* const fake_user_manager =
      new chromeos::FakeChromeUserManager;
  chromeos::ScopedUserManagerEnabler user_manager_enabler(fake_user_manager);

  const AccountId account_id(
      AccountId::FromUserEmailGaiaId("user@gmail.com", "1111111111"));
  const AccountId account_id_2(
      AccountId::FromUserEmailGaiaId("user2@gmail.com", "2222222222"));
  fake_user_manager->AddUser(account_id);
  fake_user_manager->LoginUser(account_id);
  fake_user_manager->AddUser(account_id_2);
  fake_user_manager->LoginUser(account_id_2);
  ASSERT_TRUE(testing_profile_manager.CreateTestingProfile("user@gmail.com"));
  ASSERT_TRUE(testing_profile_manager.CreateTestingProfile("user2@gmail.com"));

  GURL url;

  // Conversion of paths for removable storages.
  EXPECT_TRUE(ConvertPathToArcUrl(
      base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c"), &url));
  EXPECT_EQ(GURL("content://org.chromium.arc.removablemediaprovider/a/b/c"),
            url);

  EXPECT_FALSE(ConvertPathToArcUrl(
      base::FilePath::FromUTF8Unsafe("/media/removable_foobar"), &url));

  // Conversion of paths under the primary profile's downloads folder.
  const base::FilePath downloads = GetDownloadsFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHash(
          "user@gmail.com-hash"));
  EXPECT_TRUE(ConvertPathToArcUrl(downloads.AppendASCII("a/b/c"), &url));
  EXPECT_EQ(GURL("file:///sdcard/Download/a/b/c"), url);

  // Non-primary profile's downloads folder is not supported for ARC yet.
  const base::FilePath downloads2 = GetDownloadsFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHash(
          "user2@gmail.com-hash"));
  EXPECT_FALSE(ConvertPathToArcUrl(downloads2.AppendASCII("a/b/c"), &url));
}

}  // namespace
}  // namespace util
}  // namespace file_manager
