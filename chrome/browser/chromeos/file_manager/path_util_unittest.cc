// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/scoped_set_running_on_chromeos_for_testing.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_file_system_instance.h"
#include "components/drive/drive_pref_names.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using base::FilePath;
using storage::FileSystemURL;

namespace file_manager {
namespace util {
namespace {

const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

class FileManagerPathUtilTest : public testing::Test {
 public:
  FileManagerPathUtilTest() = default;
  ~FileManagerPathUtilTest() override = default;

  void SetUp() override {
    // Remove mount configured to MyFiles because it can interfere with some
    // tests.
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        GetDownloadsMountPointName(profile_.get()));
    profile_.reset(
        new TestingProfile(base::FilePath("/home/chronos/u-0123456789abcdef")));
  }
  void TearDown() override { profile_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileManagerPathUtilTest);
};

TEST_F(FileManagerPathUtilTest, GetDownloadsFolderForProfile) {
  std::string mount_point_name = GetDownloadsMountPointName(profile_.get());
  EXPECT_EQ("Downloads", mount_point_name);
}

TEST_F(FileManagerPathUtilTest, GetMyFilesFolderForProfile) {
  base::FilePath profile_path =
      base::FilePath("/home/chronos/u-0123456789abcdef");

  // When running outside ChromeOS, it should return $HOME/Downloads for both
  // MyFiles and Downloads.
  EXPECT_EQ(DownloadPrefs::GetDefaultDownloadDirectory(),
            GetMyFilesFolderForProfile(profile_.get()));
  EXPECT_EQ(DownloadPrefs::GetDefaultDownloadDirectory(),
            GetDownloadsFolderForProfile(profile_.get()));

  // When running inside ChromeOS, it should return /home/u-{hash}/MyFiles.
  chromeos::ScopedSetRunningOnChromeOSForTesting fake_release(kLsbRelease,
                                                              base::Time());
  EXPECT_EQ("/home/chronos/u-0123456789abcdef/MyFiles",
            GetMyFilesFolderForProfile(profile_.get()).value());
  EXPECT_EQ("/home/chronos/u-0123456789abcdef/MyFiles/Downloads",
            GetDownloadsFolderForProfile(profile_.get()).value());

  // Mount the volume to test the return from mount_points.
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      GetDownloadsMountPointName(profile_.get()),
      storage::kFileSystemTypeNativeLocal, storage::FileSystemMountOption(),
      profile_path.Append("MyFiles"));

  // When returning from the mount_point Downloads should still point to
  // MyFiles/Downloads.
  EXPECT_EQ("/home/chronos/u-0123456789abcdef/MyFiles/Downloads",
            GetDownloadsFolderForProfile(profile_.get()).value());

  // Still the same: /home/u-{hash}/MyFiles.
  EXPECT_EQ("/home/chronos/u-0123456789abcdef/MyFiles",
            GetMyFilesFolderForProfile(profile_.get()).value());
}

TEST_F(FileManagerPathUtilTest, GetPathDisplayTextForSettings) {
  EXPECT_EQ("Downloads", GetPathDisplayTextForSettings(
                             profile_.get(), "/home/chronos/user/Downloads"));
  EXPECT_EQ("Downloads",
            GetPathDisplayTextForSettings(
                profile_.get(), "/home/chronos/u-0123456789abcdef/Downloads"));

  EXPECT_EQ("My files \u203a Downloads",
            GetPathDisplayTextForSettings(
                profile_.get(), "/home/chronos/user/MyFiles/Downloads"));
  EXPECT_EQ("My files \u203a Downloads",
            GetPathDisplayTextForSettings(
                profile_.get(),
                "/home/chronos/u-0123456789abcdef/MyFiles/Downloads"));

  EXPECT_EQ("My files \u203a other-folder",
            GetPathDisplayTextForSettings(
                profile_.get(), "/home/chronos/user/MyFiles/other-folder"));
  EXPECT_EQ("My files \u203a other-folder",
            GetPathDisplayTextForSettings(
                profile_.get(),
                "/home/chronos/u-0123456789abcdef/MyFiles/other-folder"));

  EXPECT_EQ("Play files \u203a foo \u203a bar",
            GetPathDisplayTextForSettings(
                profile_.get(), "/run/arc/sdcard/write/emulated/0/foo/bar"));
  EXPECT_EQ("Linux files \u203a foo",
            GetPathDisplayTextForSettings(
                profile_.get(),
                "/media/fuse/crostini_0123456789abcdef_termina_penguin/foo"));
  EXPECT_EQ("foo", GetPathDisplayTextForSettings(profile_.get(),
                                                 "/media/removable/foo"));

  chromeos::disks::DiskMountManager::InitializeForTesting(
      new FakeDiskMountManager);
  TestingProfile profile2(base::FilePath("/home/chronos/u-0123456789abcdef"));
  chromeos::FakeChromeUserManager user_manager;
  user_manager.AddUser(
      AccountId::FromUserEmailGaiaId(profile2.GetProfileUserName(), "12345"));
  PrefService* prefs = profile2.GetPrefs();
  prefs->SetString(drive::prefs::kDriveFsProfileSalt, "a");

  drive::DriveIntegrationServiceFactory::GetForProfile(&profile2)->SetEnabled(
      true);
  EXPECT_EQ(
      "Google Drive \u203a My Drive \u203a foo",
      GetPathDisplayTextForSettings(
          &profile2,
          "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/root/foo"));
  EXPECT_EQ("Google Drive \u203a Shared drives \u203a A Team Drive \u203a foo",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                "team_drives/A Team Drive/foo"));
  EXPECT_EQ("Google Drive \u203a Computers \u203a My Other Computer \u203a bar",
            GetPathDisplayTextForSettings(
                &profile2,
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                "Computers/My Other Computer/bar"));

  EXPECT_EQ("Google Drive \u203a My Drive \u203a foo",
            GetPathDisplayTextForSettings(&profile2, "${google_drive}/foo"));

  TestingProfile guest_profile(base::FilePath("/home/chronos/guest"));
  guest_profile.SetGuestSession(true);
  guest_profile.set_profile_name("$guest");
  ASSERT_TRUE(
      drive::DriveIntegrationServiceFactory::GetForProfile(&guest_profile));

  EXPECT_EQ("Downloads", GetPathDisplayTextForSettings(
                             &guest_profile, "/home/chronos/user/Downloads"));
  // Test that a passthrough path doesn't crash on requesting the Drive mount
  // path for a guest profile.
  EXPECT_EQ("foo", GetPathDisplayTextForSettings(&guest_profile, "foo"));

  chromeos::disks::DiskMountManager::Shutdown();
}

TEST_F(FileManagerPathUtilTest, MigrateFromDownlaodsToMyFiles) {
  base::FilePath home("/home/chronos/u-0123456789abcdef");
  base::FilePath result;
  base::FilePath downloads = home.Append("Downloads");
  base::FilePath file = home.Append("Downloads/file.txt");
  base::FilePath inhome = home.Append("NotDownloads");
  base::FilePath myfiles = home.Append("MyFiles");
  base::FilePath myfilesFile = home.Append("MyFiles/file.txt");
  base::FilePath myfilesDownloads = home.Append("MyFiles/Downloads");
  base::FilePath myfilesDownloadsFile =
      home.Append("MyFiles/Downloads/file.txt");
  base::FilePath other("/some/other/path");
  chromeos::ScopedSetRunningOnChromeOSForTesting fake_release(kLsbRelease,
                                                              base::Time());
  // MyFilesVolume enabled, migrate paths under Downloads.
  EXPECT_TRUE(
      MigrateFromDownloadsToMyFiles(profile_.get(), downloads, &result));
  EXPECT_EQ(result, myfilesDownloads);
  EXPECT_TRUE(MigrateFromDownloadsToMyFiles(profile_.get(), file, &result));
  EXPECT_EQ(result, myfilesDownloadsFile);
  EXPECT_FALSE(MigrateFromDownloadsToMyFiles(profile_.get(), inhome, &result));
  EXPECT_FALSE(MigrateFromDownloadsToMyFiles(profile_.get(), myfiles, &result));
  EXPECT_FALSE(
      MigrateFromDownloadsToMyFiles(profile_.get(), myfilesFile, &result));
  EXPECT_FALSE(
      MigrateFromDownloadsToMyFiles(profile_.get(), myfilesDownloads, &result));
  EXPECT_FALSE(MigrateFromDownloadsToMyFiles(profile_.get(),
                                             myfilesDownloadsFile, &result));
  EXPECT_FALSE(MigrateFromDownloadsToMyFiles(profile_.get(), other, &result));
}

TEST_F(FileManagerPathUtilTest, MultiProfileDownloadsFolderMigration) {
  // MigratePathFromOldFormat is explicitly disabled on Linux build.
  // So we need to fake that this is real ChromeOS system.
  chromeos::ScopedSetRunningOnChromeOSForTesting fake_release(kLsbRelease,
                                                              base::Time());

  // /home/chronos/u-${HASH}/MyFiles/Downloads
  const FilePath kDownloadsFolder =
      GetDownloadsFolderForProfile(profile_.get());
  // /home/chronos/u-${HASH}/MyFiles/
  const FilePath kMyFilesFolder = GetMyFilesFolderForProfile(profile_.get());
  // In the device: /home/chronos/user
  // In browser tests: /tmp/.org.chromium.Chromium.F0Ejp5
  const FilePath old_base = DownloadPrefs::GetDefaultDownloadDirectory();

  FilePath path;

  // Special case to convert the base pkth directly to MyFiles/Downloads,
  // because DownloadPrefs is initially initialized to /home/chronos/user before
  // we have the Profile fully set up and we want to set it to MyFiles/Downloads
  // which is the default download folder for new users.
  EXPECT_TRUE(MigratePathFromOldFormat(profile_.get(),
                                       FilePath("/home/chronos/user"),
                                       FilePath("/home/chronos/user"), &path));
  EXPECT_EQ(kDownloadsFolder, path);

  EXPECT_TRUE(
      MigratePathFromOldFormat(profile_.get(), FilePath("/home/chronos/user"),
                               FilePath("/home/chronos/user/a/b"), &path));
  EXPECT_EQ(kMyFilesFolder.AppendASCII("a/b"), path);
  EXPECT_TRUE(
      MigratePathFromOldFormat(profile_.get(), FilePath("/home/chronos/u-1234"),
                               FilePath("/home/chronos/u-1234/a/b"), &path));
  EXPECT_EQ(kMyFilesFolder.AppendASCII("a/b"), path);

  // Path already in the new format is not converted, it's already inside
  // MyFiles or MyFiles/Downloads.
  EXPECT_FALSE(MigratePathFromOldFormat(
      profile_.get(), DownloadPrefs::GetDefaultDownloadDirectory(),
      kMyFilesFolder.AppendASCII("a/b"), &path));
  EXPECT_FALSE(MigratePathFromOldFormat(profile_.get(), kMyFilesFolder,
                                        kMyFilesFolder.AppendASCII("a/b"),
                                        &path));
  EXPECT_FALSE(MigratePathFromOldFormat(
      profile_.get(), DownloadPrefs::GetDefaultDownloadDirectory(),
      kDownloadsFolder.AppendASCII("a/b"), &path));
  EXPECT_FALSE(MigratePathFromOldFormat(profile_.get(), kMyFilesFolder,
                                        kDownloadsFolder.AppendASCII("a/b"),
                                        &path));

  // Only /home/chronos/user is migrated when old_base == old_path.
  EXPECT_FALSE(
      MigratePathFromOldFormat(profile_.get(), FilePath("/home/chronos/u-1234"),
                               FilePath("/home/chronos/u-1234"), &path));
  // Won't migrate because old_path isn't inside the default downloads
  // directory.
  EXPECT_FALSE(MigratePathFromOldFormat(
      profile_.get(), DownloadPrefs::GetDefaultDownloadDirectory(),
      FilePath::FromUTF8Unsafe("/home/chronos/user/dl"), &path));
}

TEST_F(FileManagerPathUtilTest, MigrateToDriveFs) {
  base::FilePath home("/home/chronos/u-0123456789abcdef");
  base::FilePath other("/some/other/path");
  base::FilePath old_drive("/special/drive-0123456789abcdef");
  base::FilePath my_drive = old_drive.Append("root");
  base::FilePath file_in_my_drive = old_drive.Append("root").Append("file.txt");

  // Migrate paths under old drive mount.
  TestingProfile profile2(base::FilePath("/home/chronos/u-0123456789abcdef"));
  chromeos::FakeChromeUserManager user_manager;
  user_manager.AddUser(
      AccountId::FromUserEmailGaiaId(profile2.GetProfileUserName(), "12345"));
  PrefService* prefs = profile2.GetPrefs();
  prefs->SetString(drive::prefs::kDriveFsProfileSalt, "a");
  drive::DriveIntegrationServiceFactory::GetForProfile(&profile2)->SetEnabled(
      true);

  base::FilePath result;
  EXPECT_FALSE(MigrateToDriveFs(&profile2, other, &result));
  EXPECT_TRUE(MigrateToDriveFs(&profile2, my_drive, &result));
  EXPECT_EQ(base::FilePath(
                "/media/fuse/drivefs-84675c855b63e12f384d45f033826980/root"),
            result);
  EXPECT_TRUE(MigrateToDriveFs(&profile2, file_in_my_drive, &result));
  EXPECT_EQ(
      base::FilePath("/media/fuse/drivefs-84675c855b63e12f384d45f033826980/"
                     "root/file.txt"),
      result);
}

TEST_F(FileManagerPathUtilTest, ConvertFileSystemURLToPathInsideCrostini) {
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  // Setup for DriveFS.
  chromeos::FakeChromeUserManager user_manager;
  user_manager.AddUser(
      AccountId::FromUserEmailGaiaId(profile_->GetProfileUserName(), "12345"));
  profile_->GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt, "a");

  // Initialize DBUS and running container.
  chromeos::DBusThreadManager::Initialize();
  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(profile_.get());
  crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
  crostini_manager->AddRunningContainerForTesting(
      crostini::kCrostiniDefaultVmName,
      crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                              "testuser", "/home/testuser", "PLACEHOLDER_IP"));
  //
  // Register crostini, downloads, drive, android.
  mount_points->RegisterFileSystem(GetCrostiniMountPointName(profile_.get()),
                                   storage::kFileSystemTypeNativeLocal,
                                   storage::FileSystemMountOption(),
                                   GetCrostiniMountDirectory(profile_.get()));
  mount_points->RegisterFileSystem(
      GetDownloadsMountPointName(profile_.get()),
      storage::kFileSystemTypeNativeLocal, storage::FileSystemMountOption(),
      GetDownloadsFolderForProfile(profile_.get()));
  mount_points->RegisterFileSystem(
      GetAndroidFilesMountPointName(), storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(), base::FilePath(kAndroidFilesPath));
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_.get());
  base::FilePath mount_point_drive = integration_service->GetMountPointPath();
  mount_points->RegisterFileSystem(
      mount_point_drive.BaseName().value(), storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(), mount_point_drive);
  mount_points->RegisterFileSystem(
      chromeos::kSystemMountNameRemovable, storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(), base::FilePath(kRemovableMediaPath));

  {
    base::FilePath inside;
    EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
        profile_.get(),
        mount_points->CreateExternalFileSystemURL(
            url::Origin(), "crostini_0123456789abcdef_termina_penguin",
            base::FilePath("path/in/crostini")),
        &inside));
    EXPECT_EQ("/home/testuser/path/in/crostini", inside.value());

    EXPECT_FALSE(ConvertFileSystemURLToPathInsideCrostini(
        profile_.get(),
        mount_points->CreateExternalFileSystemURL(
            url::Origin(), "unknown", base::FilePath("path/in/unknown")),
        &inside));

    EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
        profile_.get(),
        mount_points->CreateExternalFileSystemURL(
            url::Origin(), "android_files", base::FilePath("path/in/android")),
        &inside));
    EXPECT_EQ("/mnt/chromeos/PlayFiles/path/in/android", inside.value());

    EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
        profile_.get(),
        mount_points->CreateExternalFileSystemURL(
            url::Origin(), "drivefs-84675c855b63e12f384d45f033826980",
            base::FilePath("root/path/in/mydrive")),
        &inside));
    EXPECT_EQ("/mnt/chromeos/GoogleDrive/MyDrive/path/in/mydrive",
              inside.value());

    EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
        profile_.get(),
        mount_points->CreateExternalFileSystemURL(
            url::Origin(), "drivefs-84675c855b63e12f384d45f033826980",
            base::FilePath("team_drives/path/in/teamdrives")),
        &inside));
    EXPECT_EQ("/mnt/chromeos/GoogleDrive/SharedDrives/path/in/teamdrives",
              inside.value());

    EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
        profile_.get(),
        mount_points->CreateExternalFileSystemURL(
            url::Origin(), "drivefs-84675c855b63e12f384d45f033826980",
            base::FilePath("Computers/path/in/computers")),
        &inside));
    EXPECT_EQ("/mnt/chromeos/GoogleDrive/Computers/path/in/computers",
              inside.value());

    EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
        profile_.get(),
        mount_points->CreateExternalFileSystemURL(
            url::Origin(), "removable",
            base::FilePath("MyUSB/path/in/removable")),
        &inside));
    EXPECT_EQ("/mnt/chromeos/removable/MyUSB/path/in/removable",
              inside.value());
  }
  // Test MyFiles.
  mount_points->RegisterFileSystem(GetDownloadsMountPointName(profile_.get()),
                                   storage::kFileSystemTypeNativeLocal,
                                   storage::FileSystemMountOption(),
                                   GetMyFilesFolderForProfile(profile_.get()));
  base::FilePath inside;
  EXPECT_TRUE(ConvertFileSystemURLToPathInsideCrostini(
      profile_.get(),
      mount_points->CreateExternalFileSystemURL(
          url::Origin(), "Downloads-testing_profile-hash",
          base::FilePath("path/in/myfiles")),
      &inside));
  EXPECT_EQ("/mnt/chromeos/MyFiles/path/in/myfiles", inside.value());
}

TEST_F(FileManagerPathUtilTest, ExtractMountNameFileSystemNameFullPath) {
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  std::string downloads_mount_name = GetDownloadsMountPointName(profile_.get());
  base::FilePath downloads = GetDownloadsFolderForProfile(profile_.get());
  mount_points->RegisterFileSystem(downloads_mount_name,
                                   storage::kFileSystemTypeNativeLocal,
                                   storage::FileSystemMountOption(), downloads);
  base::FilePath removable = base::FilePath(kRemovableMediaPath);
  mount_points->RegisterFileSystem(
      chromeos::kSystemMountNameRemovable, storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(), base::FilePath(kRemovableMediaPath));
  std::string relative_path_1 = "foo";
  std::string relative_path_2 = "foo/bar";
  std::string mount_name;
  std::string file_system_name;
  std::string full_path;

  // <Downloads>/
  EXPECT_TRUE(ExtractMountNameFileSystemNameFullPath(
      downloads, &mount_name, &file_system_name, &full_path));
  EXPECT_EQ(mount_name, downloads_mount_name);
  EXPECT_EQ(file_system_name, downloads_mount_name);
  EXPECT_EQ(full_path, "/");

  // <Downloads>/foo
  EXPECT_TRUE(ExtractMountNameFileSystemNameFullPath(
      downloads.Append(relative_path_1), &mount_name, &file_system_name,
      &full_path));
  EXPECT_EQ(mount_name, downloads_mount_name);
  EXPECT_EQ(file_system_name, downloads_mount_name);
  EXPECT_EQ(full_path, "/foo");

  // <Downloads>/foo/bar
  EXPECT_TRUE(ExtractMountNameFileSystemNameFullPath(
      downloads.Append(relative_path_2), &mount_name, &file_system_name,
      &full_path));
  EXPECT_EQ(mount_name, downloads_mount_name);
  EXPECT_EQ(file_system_name, downloads_mount_name);
  EXPECT_EQ(full_path, "/foo/bar");

  // <removable>/
  EXPECT_FALSE(ExtractMountNameFileSystemNameFullPath(
      removable, &mount_name, &file_system_name, &full_path));

  // <removable>/foo/
  EXPECT_TRUE(ExtractMountNameFileSystemNameFullPath(
      removable.Append(relative_path_1), &mount_name, &file_system_name,
      &full_path));
  EXPECT_EQ(mount_name, "removable/foo");
  EXPECT_EQ(file_system_name, "foo");
  EXPECT_EQ(full_path, "/");

  // <removable>/foo/bar
  EXPECT_TRUE(ExtractMountNameFileSystemNameFullPath(
      removable.Append(relative_path_2), &mount_name, &file_system_name,
      &full_path));
  EXPECT_EQ(mount_name, "removable/foo");
  EXPECT_EQ(file_system_name, "foo");
  EXPECT_EQ(full_path, "/bar");

  // Unknown.
  EXPECT_FALSE(ExtractMountNameFileSystemNameFullPath(
      base::FilePath("/unknown/path"), &mount_name, &file_system_name,
      &full_path));
}

std::unique_ptr<KeyedService> CreateFileSystemOperationRunnerForTesting(
    content::BrowserContext* context) {
  return arc::ArcFileSystemOperationRunner::CreateForTesting(
      context, arc::ArcServiceManager::Get()->arc_bridge_service());
}

class FileManagerPathUtilConvertUrlTest : public testing::Test {
 public:
  FileManagerPathUtilConvertUrlTest() = default;
  ~FileManagerPathUtilConvertUrlTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    // Set up fake user manager.
    chromeos::FakeChromeUserManager* fake_user_manager =
        new chromeos::FakeChromeUserManager();
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId("user@gmail.com", "1111111111"));
    const AccountId account_id_2(
        AccountId::FromUserEmailGaiaId("user2@gmail.com", "2222222222"));
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);
    fake_user_manager->AddUser(account_id_2);
    fake_user_manager->LoginUser(account_id_2);
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(std::move(fake_user_manager)));

    Profile* primary_profile =
        profile_manager_->CreateTestingProfile("user@gmail.com");
    ASSERT_TRUE(primary_profile);
    ASSERT_TRUE(profile_manager_->CreateTestingProfile("user2@gmail.com"));
    primary_profile->GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt,
                                           "a");
    primary_profile->GetPrefs()->SetBoolean(
        drive::prefs::kDriveFsPinnedMigrated, true);

    // Set up an Arc service manager with a fake file system.
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    arc_service_manager_->set_browser_context(primary_profile);
    arc::ArcFileSystemOperationRunner::GetFactory()->SetTestingFactoryAndUse(
        primary_profile,
        base::BindRepeating(&CreateFileSystemOperationRunnerForTesting));
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &fake_file_system_);
    arc::WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());
    ASSERT_TRUE(fake_file_system_.InitCalled());

    // Add a drive mount point for the primary profile.
    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    drive::DriveIntegrationService* integration_service =
        drive::DriveIntegrationServiceFactory::GetForProfile(primary_profile);
    drive_mount_point_ = integration_service->GetMountPointPath();
    integration_service->OnMounted(drive_mount_point_);

    // Add a crostini mount point for the primary profile.
    crostini_mount_point_ = GetCrostiniMountDirectory(primary_profile);
    mount_points->RegisterFileSystem(GetCrostiniMountPointName(primary_profile),
                                     storage::kFileSystemTypeNativeLocal,
                                     storage::FileSystemMountOption(),
                                     crostini_mount_point_);

    // Run pending async tasks resulting from profile construction to ensure
    // these are complete before the test begins.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &fake_file_system_);
    user_manager_enabler_.reset();
    profile_manager_.reset();

    // Run all pending tasks before destroying testing profile.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  arc::FakeFileSystemInstance fake_file_system_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  base::FilePath drive_mount_point_;
  base::FilePath crostini_mount_point_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileManagerPathUtilConvertUrlTest);
};

FileSystemURL CreateExternalURL(const base::FilePath& path) {
  return FileSystemURL::CreateForTest(url::Origin(),
                                      storage::kFileSystemTypeExternal, path);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_Removable) {
  GURL url;
  EXPECT_TRUE(ConvertPathToArcUrl(
      base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c"), &url));
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/removable/a/b/c"),
            url);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_MyFiles) {
  chromeos::ScopedSetRunningOnChromeOSForTesting fake_release(kLsbRelease,
                                                              base::Time());
  GURL url;
  const base::FilePath myfiles = GetMyFilesFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHashForTest(
          "user@gmail.com-hash"));
  EXPECT_TRUE(ConvertPathToArcUrl(myfiles.AppendASCII("a/b/c"), &url));
  EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/MyFiles/a/b/c"),
            url);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertPathToArcUrl_InvalidRemovable) {
  GURL url;
  EXPECT_FALSE(ConvertPathToArcUrl(
      base::FilePath::FromUTF8Unsafe("/media/removable_foobar"), &url));
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertPathToArcUrl_InvalidDownloads) {
  // Non-primary profile's downloads folder is not supported for ARC yet.
  GURL url;
  const base::FilePath downloads2 = GetDownloadsFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHashForTest(
          "user2@gmail.com-hash"));
  EXPECT_FALSE(ConvertPathToArcUrl(downloads2.AppendASCII("a/b/c"), &url));
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_Crostini) {
  GURL url;
  EXPECT_TRUE(
      ConvertPathToArcUrl(crostini_mount_point_.AppendASCII("a/b/c"), &url));
  EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                 "externalfile%3A"
                 "crostini_user%40gmail.com-hash_termina_penguin%2Fa%2Fb%2Fc"),
            url);
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertPathToArcUrl_Special) {
  GURL url;
  EXPECT_TRUE(
      ConvertPathToArcUrl(drive_mount_point_.AppendASCII("a/b/c"), &url));
  // "@" appears escaped 3 times here because escaping happens when:
  // - creating drive mount point name for user
  // - creating externalfile: URL from the path
  // - encoding the URL to Chrome content provider URL
  EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                 "externalfile%3Adrivefs-b1f44746e7144c3caafeacaa8bb5c569%2Fa"
                 "%2Fb%2Fc"),
            url);
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidMountType) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeTest,
          base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_Removable) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{CreateExternalURL(
          base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                           "removable/a/b/c"),
                      urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_MyFiles) {
  chromeos::ScopedSetRunningOnChromeOSForTesting fake_release(kLsbRelease,
                                                              base::Time());
  const base::FilePath myfiles = GetMyFilesFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHashForTest(
          "user@gmail.com-hash"));
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{
          CreateExternalURL(myfiles.AppendASCII("a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(
                GURL("content://org.chromium.arc.volumeprovider/MyFiles/a/b/c"),
                urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidRemovable) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{CreateExternalURL(
          base::FilePath::FromUTF8Unsafe("/media/removable_foobar"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_Downloads) {
  const base::FilePath downloads = GetDownloadsFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHashForTest(
          "user@gmail.com-hash"));
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{
          CreateExternalURL(downloads.AppendASCII("a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(
                GURL("content://org.chromium.arc.file_system.fileprovider/"
                     "download/a/b/c"),
                urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidDownloads) {
  const base::FilePath downloads = GetDownloadsFolderForProfile(
      chromeos::ProfileHelper::Get()->GetProfileByUserIdHashForTest(
          "user2@gmail.com-hash"));
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{
          CreateExternalURL(downloads.AppendASCII("a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_Special) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{
          CreateExternalURL(drive_mount_point_.AppendASCII("a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                           "externalfile%3Adrivefs-b1f44746e7144c3caafeacaa8bb5"
                           "c569%2Fa%2Fb%2Fc"),
                      urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_ArcDocumentsProvider) {
  // Add images_root/Download/photo.jpg to the fake file system.
  const char kAuthority[] = "com.android.providers.media.documents";
  fake_file_system_.AddDocument(arc::FakeFileSystemInstance::Document(
      kAuthority, "images_root", "", "", arc::kAndroidDirectoryMimeType, -1,
      0));
  fake_file_system_.AddDocument(arc::FakeFileSystemInstance::Document(
      kAuthority, "dir-id", "images_root", "Download",
      arc::kAndroidDirectoryMimeType, -1, 22));
  fake_file_system_.AddDocument(arc::FakeFileSystemInstance::Document(
      kAuthority, "photo-id", "dir-id", "photo.jpg", "image/jpeg", 3, 33));

  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath::FromUTF8Unsafe(
              "/special/arc-documents-provider/"
              "com.android.providers.media.documents/"
              "images_root/Download/photo.jpg"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL("content://com.android.providers.media.documents/"
                           "document/photo-id"),
                      urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_ArcDocumentsProviderFileNotFound) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath::FromUTF8Unsafe(
              "/special/arc-documents-provider/"
              "com.android.providers.media.documents/"
              "images_root/Download/photo.jpg"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(""), urls[0]);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_AndroidFiles) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{
          CreateExternalURL(base::FilePath::FromUTF8Unsafe(
              "/run/arc/sdcard/write/emulated/0/Pictures/a/b.jpg"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(
                GURL("content://org.chromium.arc.file_system.fileprovider/"
                     "external_files/Pictures/a/b.jpg"),
                urls[0]);
          },
          &run_loop));
}

TEST_F(FileManagerPathUtilConvertUrlTest,
       ConvertToContentUrls_InvalidAndroidFiles) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{
          CreateExternalURL(base::FilePath::FromUTF8Unsafe(
              "/run/arc/sdcard/read/emulated/0/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(1U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);  // Invalid URL.
          },
          &run_loop));
}

TEST_F(FileManagerPathUtilConvertUrlTest, ConvertToContentUrls_MultipleUrls) {
  base::RunLoop run_loop;
  ConvertToContentUrls(
      std::vector<FileSystemURL>{
          CreateExternalURL(base::FilePath::FromUTF8Unsafe("/invalid")),
          CreateExternalURL(
              base::FilePath::FromUTF8Unsafe("/media/removable/a/b/c")),
          CreateExternalURL(drive_mount_point_.AppendASCII("a/b/c")),
          CreateExternalURL(base::FilePath::FromUTF8Unsafe(
              "/run/arc/sdcard/write/emulated/0/a/b/c"))},
      base::BindOnce(
          [](base::RunLoop* run_loop, const std::vector<GURL>& urls) {
            run_loop->Quit();
            ASSERT_EQ(4U, urls.size());
            EXPECT_EQ(GURL(), urls[0]);  // Invalid URL.
            EXPECT_EQ(GURL("content://org.chromium.arc.volumeprovider/"
                           "removable/a/b/c"),
                      urls[1]);
            EXPECT_EQ(GURL("content://org.chromium.arc.chromecontentprovider/"
                           "externalfile%3Adrivefs-b1f44746e7144c3caafeacaa8bb5"
                           "c569%2Fa%2Fb%2Fc"),
                      urls[2]);
            EXPECT_EQ(
                GURL("content://org.chromium.arc.file_system.fileprovider/"
                     "external_files/a/b/c"),
                urls[3]);
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace
}  // namespace util
}  // namespace file_manager
