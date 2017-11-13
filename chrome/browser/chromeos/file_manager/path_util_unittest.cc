// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/scoped_set_running_on_chromeos_for_testing.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace util {
namespace {

const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

TEST(FileManagerPathUtilTest, MultiProfileDownloadsFolderMigration) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;
  // MigratePathFromOldFormat is explicitly disabled on Linux build.
  // So we need to fake that this is real ChromeOS system.
  chromeos::ScopedSetRunningOnChromeOSForTesting fake_release(kLsbRelease,
                                                              base::Time());

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
  user_manager::ScopedUserManager user_manager_enabler(
      base::WrapUnique(fake_user_manager));

  const AccountId account_id(
      AccountId::FromUserEmailGaiaId("user@gmail.com", "1111111111"));
  const AccountId account_id_2(
      AccountId::FromUserEmailGaiaId("user2@gmail.com", "2222222222"));
  fake_user_manager->AddUser(account_id);
  fake_user_manager->LoginUser(account_id);
  fake_user_manager->AddUser(account_id_2);
  fake_user_manager->LoginUser(account_id_2);
  Profile* primary_profile =
      testing_profile_manager.CreateTestingProfile("user@gmail.com");
  ASSERT_TRUE(primary_profile);
  ASSERT_TRUE(testing_profile_manager.CreateTestingProfile("user2@gmail.com"));

  // Add a Drive mount point for the primary profile.
  const base::FilePath drive_mount_point =
      drive::util::GetDriveMountPointPath(primary_profile);
  const std::string mount_name = drive_mount_point.BaseName().AsUTF8Unsafe();
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      mount_name, storage::kFileSystemTypeDrive,
      storage::FileSystemMountOption(), drive_mount_point);

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
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHashForTest(
          "user@gmail.com-hash"));
  EXPECT_TRUE(ConvertPathToArcUrl(downloads.AppendASCII("a/b/c"), &url));
  EXPECT_EQ(GURL("content://org.chromium.arc.intent_helper.fileprovider/"
                 "download/a/b/c"),
            url);

  // Non-primary profile's downloads folder is not supported for ARC yet.
  const base::FilePath downloads2 = GetDownloadsFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHashForTest(
          "user2@gmail.com-hash"));
  EXPECT_FALSE(ConvertPathToArcUrl(downloads2.AppendASCII("a/b/c"), &url));

  // Conversion of paths under /special.
  EXPECT_TRUE(
      ConvertPathToArcUrl(drive_mount_point.AppendASCII("a/b/c"), &url));
  EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                 "externalfile%3Adrive-user%2540gmail.com-hash%2Fa%2Fb%2Fc"),
            url);
}

}  // namespace
}  // namespace util
}  // namespace file_manager
