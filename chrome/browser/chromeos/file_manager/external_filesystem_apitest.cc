// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/mount_test_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/cast_config_client_media_router.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/test/result_catcher.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "google_apis/drive/time_util.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

// Tests for access to external file systems (as defined in
// storage/common/fileapi/file_system_types.h) from extensions with
// fileManagerPrivate and fileBrowserHandler extension permissions.
// The tests cover following external file system types:
// - local (kFileSystemTypeLocalNative): a local file system on which files are
//   accessed using native local path.
// - restricted (kFileSystemTypeRestrictedLocalNative): a *read-only* local file
//   system which can only be accessed by extensions that have full access to
//   external file systems (i.e. extensions with fileManagerPrivate permission).
// - drive (kFileSystemTypeDrive): a file system that provides access to Google
//   Drive.
//
// The tests cover following scenarios:
// - Performing file system operations on external file systems from an
//   app with fileManagerPrivate permission (i.e. The Files app).
// - Performing read/write operations from file handler extensions. These
//   extensions need a file browser extension to give them permissions to access
//   files. This also includes file handler extensions in filesystem API.
// - Observing directory changes from a file browser extension (using
//   fileManagerPrivate API).
// - Doing searches on drive file system from file browser extension (using
//   fileManagerPrivate API).

using drive::DriveIntegrationServiceFactory;
using extensions::Extension;

namespace file_manager {
namespace {

// Root dirs for file systems expected by the test extensions.
// NOTE: Root dir for drive file system is set by Chrome's drive implementation,
// but the test will have to make sure the mount point is added before
// starting a test extension using WaitUntilDriveMountPointIsAdded().
const char kLocalMountPointName[] = "local";
const char kRestrictedMountPointName[] = "restricted";

// Default file content for the test files.
const char kTestFileContent[] = "This is some test content.";

// User account email and directory hash for secondary account for multi-profile
// sensitive test cases.
const char kSecondProfileAccount[] = "profile2@test.com";
const char kSecondProfileHash[] = "fileBrowserApiTestProfile2";

class FakeSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeSelectFileDialog(ui::SelectFileDialog::Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)) {}

  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    listener_->FileSelected(base::FilePath("/special/drive-user/root/test_dir"),
                            0, nullptr);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }

  void ListenerDestroyed() override {}

  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~FakeSelectFileDialog() override {}
};

class FakeSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 private:
  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new FakeSelectFileDialog(listener, std::move(policy));
  }
};

// Sets up the initial file system state for native local and restricted native
// local file systems. The hierarchy is the same as for the drive file system.
// The directory is created at unique_temp_dir/|mount_point_name| path.
bool InitializeLocalFileSystem(std::string mount_point_name,
                               base::ScopedTempDir* tmp_dir,
                               base::FilePath* mount_point_dir) {
  if (!tmp_dir->CreateUniqueTempDir())
    return false;

  *mount_point_dir = tmp_dir->GetPath().AppendASCII(mount_point_name);
  // Create the mount point.
  if (!base::CreateDirectory(*mount_point_dir))
    return false;

  base::FilePath test_dir = mount_point_dir->AppendASCII("test_dir");
  if (!base::CreateDirectory(test_dir))
    return false;

  base::FilePath test_subdir = test_dir.AppendASCII("empty_test_dir");
  if (!base::CreateDirectory(test_subdir))
    return false;

  test_subdir = test_dir.AppendASCII("subdir");
  if (!base::CreateDirectory(test_subdir))
    return false;

  base::FilePath test_file = test_dir.AppendASCII("test_file.xul");
  if (!google_apis::test_util::WriteStringToFile(test_file, kTestFileContent))
    return false;

  test_file = test_dir.AppendASCII("test_file.xul.foo");
  if (!google_apis::test_util::WriteStringToFile(test_file, kTestFileContent))
    return false;

  test_file = test_dir.AppendASCII("test_file.tiff");
  if (!google_apis::test_util::WriteStringToFile(test_file, kTestFileContent))
    return false;

  test_file = test_dir.AppendASCII("test_file.tiff.foo");
  if (!google_apis::test_util::WriteStringToFile(test_file, kTestFileContent))
    return false;

  test_file = test_dir.AppendASCII("empty_test_file.foo");
  if (!google_apis::test_util::WriteStringToFile(test_file, ""))
    return false;

  return true;
}

void IgnoreDriveEntryResult(google_apis::DriveApiErrorCode error,
                            std::unique_ptr<google_apis::FileResource> entry) {}

void UpdateDriveEntryTime(drive::FakeDriveService* fake_drive_service,
                          const std::string& resource_id,
                          const std::string& last_modified,
                          const std::string& last_viewed_by_me) {
  base::Time last_modified_time, last_viewed_by_me_time;
  ASSERT_TRUE(google_apis::util::GetTimeFromString(last_modified,
                                                   &last_modified_time) &&
              google_apis::util::GetTimeFromString(last_viewed_by_me,
                                                   &last_viewed_by_me_time));
  fake_drive_service->UpdateResource(resource_id,
                                     std::string(),  // parent_resource_id
                                     std::string(),  // title
                                     last_modified_time, last_viewed_by_me_time,
                                     google_apis::drive::Properties(),
                                     base::Bind(&IgnoreDriveEntryResult));
}

void AddFileToDriveService(drive::FakeDriveService* fake_drive_service,
                           const std::string& mime_type,
                           const std::string& content,
                           const std::string& parent_resource_id,
                           const std::string& title,
                           const std::string& last_modified,
                           const std::string& last_viewed_by_me) {
  fake_drive_service->AddNewFileWithResourceId(
      title, mime_type, content, parent_resource_id, title,
      false,  // shared_with_me
      base::Bind(&IgnoreDriveEntryResult));
  UpdateDriveEntryTime(fake_drive_service, title, last_modified,
                       last_viewed_by_me);
}

void AddDirectoryToDriveService(drive::FakeDriveService* fake_drive_service,
                                const std::string& parent_resource_id,
                                const std::string& title,
                                const std::string& last_modified,
                                const std::string& last_viewed_by_me) {
  fake_drive_service->AddNewDirectoryWithResourceId(
      title, parent_resource_id, title, drive::AddNewDirectoryOptions(),
      base::Bind(&IgnoreDriveEntryResult));
  UpdateDriveEntryTime(fake_drive_service, title, last_modified,
                       last_viewed_by_me);
}

// Sets up the drive service state.
// The hierarchy is the same as for the local file system.
drive::FakeDriveService* CreateDriveService() {
  drive::FakeDriveService* service = new drive::FakeDriveService;
  service->LoadAppListForDriveApi("drive/applist.json");
  AddDirectoryToDriveService(service, service->GetRootResourceId(), "test_dir",
                             "2012-01-02T00:00:00.000Z",
                             "2012-01-02T00:00:01.000Z");
  AddDirectoryToDriveService(service, "test_dir", "empty_test_dir",
                             "2011-11-02T04:00:00.000Z",
                             "2011-11-02T04:00:00.000Z");
  AddDirectoryToDriveService(service, "test_dir", "subdir",
                             "2011-04-01T18:34:08.234Z",
                             "2012-01-02T00:00:01.000Z");
  AddFileToDriveService(service, "application/vnd.mozilla.xul+xml",
                        kTestFileContent, "test_dir", "test_file.xul",
                        "2011-12-14T00:40:47.330Z", "2012-01-02T00:00:00.000Z");
  AddFileToDriveService(service, "test/ro", kTestFileContent, "test_dir",
                        "test_file.xul.foo", "2012-01-01T10:00:30.000Z",
                        "2012-01-01T00:00:00.000Z");
  AddFileToDriveService(service, "image/tiff", kTestFileContent, "test_dir",
                        "test_file.tiff", "2011-04-03T11:11:10.000Z",
                        "2012-01-02T00:00:00.000Z");
  AddFileToDriveService(service, "test/rw", kTestFileContent, "test_dir",
                        "test_file.tiff.foo", "2011-12-14T00:40:47.330Z",
                        "2010-01-02T00:00:00.000Z");
  AddFileToDriveService(service, "test/rw", "", "test_dir",
                        "empty_test_file.foo", "2011-12-14T00:40:47.330Z",
                        "2011-12-14T00:40:47.330Z");
  return service;
}

// Helper class to wait for a background page to load or close again.
class BackgroundObserver {
 public:
  BackgroundObserver()
      : page_created_(extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                      content::NotificationService::AllSources()),
        page_closed_(extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                     content::NotificationService::AllSources()) {}

  void WaitUntilLoaded() {
    page_created_.Wait();
  }

  void WaitUntilClosed() {
    page_closed_.Wait();
  }

 private:
  content::WindowedNotificationObserver page_created_;
  content::WindowedNotificationObserver page_closed_;
};

// Base class for FileSystemExtensionApi tests.
class FileSystemExtensionApiTestBase : public ExtensionApiTest {
 public:
  enum Flags {
    FLAGS_NONE = 0,
    FLAGS_USE_FILE_HANDLER = 1 << 1,
    FLAGS_LAZY_FILE_HANDLER = 1 << 2
  };

  FileSystemExtensionApiTestBase() {}
  ~FileSystemExtensionApiTestBase() override {}

  void SetUp() override {
    InitTestFileSystem();
    ExtensionApiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    AddTestMountPoint();

    // Mock the Media Router in extension api tests. Dispatches to the message
    // loop now try to handle mojo messages that will call back into Profile
    // creation through the media router, which then confuse the drive code.
    ON_CALL(media_router_, RegisterMediaSinksObserver(testing::_))
        .WillByDefault(testing::Return(true));
    CastConfigClientMediaRouter::SetMediaRouterForTest(&media_router_);

    ExtensionApiTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    CastConfigClientMediaRouter::SetMediaRouterForTest(nullptr);
    ExtensionApiTest::TearDownOnMainThread();
  }

  // Runs a file system extension API test.
  // It loads test component extension at |filebrowser_path| with manifest
  // at |filebrowser_manifest|. The |filebrowser_manifest| should be a path
  // relative to |filebrowser_path|. The method waits until the test extension
  // sends test succeed or fail message. It returns true if the test succeeds.
  // If |FLAGS_USE_FILE_HANDLER| flag is set, the file handler extension at path
  // |filehandler_path| will be loaded before the file browser extension.
  // If the flag FLAGS_LAZY_FILE_HANDLER is set, the file handler extension must
  // not have persistent background page. The test will wait until the file
  // handler's background page is closed after initial load before the file
  // browser extension is loaded.
  // If |RunFileSystemExtensionApiTest| fails, |message_| will contain a failure
  // message.
  bool RunFileSystemExtensionApiTest(
      const std::string& filebrowser_path,
      const base::FilePath::CharType* filebrowser_manifest,
      const std::string& filehandler_path,
      int flags) {
    if (flags & FLAGS_USE_FILE_HANDLER) {
      if (filehandler_path.empty()) {
        message_ = "Missing file handler path.";
        return false;
      }

      BackgroundObserver page_complete;
      const Extension* file_handler =
          LoadExtension(test_data_dir_.AppendASCII(filehandler_path));
      if (!file_handler)
        return false;

      if (flags & FLAGS_LAZY_FILE_HANDLER) {
        page_complete.WaitUntilClosed();
      } else {
        page_complete.WaitUntilLoaded();
      }
    }

    extensions::ResultCatcher catcher;

    const Extension* file_browser = LoadExtensionAsComponentWithManifest(
        test_data_dir_.AppendASCII(filebrowser_path),
        filebrowser_manifest);
    if (!file_browser)
      return false;

    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }

    return true;
  }

 protected:
  // Sets up initial test file system hierarchy.
  virtual void InitTestFileSystem() = 0;
  // Registers mount point used in the test.
  virtual void AddTestMountPoint() = 0;

 private:
  media_router::MockMediaRouter media_router_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemExtensionApiTestBase);
};

// Tests for a native local file system.
class LocalFileSystemExtensionApiTest : public FileSystemExtensionApiTestBase {
 public:
  LocalFileSystemExtensionApiTest() {}
  ~LocalFileSystemExtensionApiTest() override {}

  // FileSystemExtensionApiTestBase override.
  void InitTestFileSystem() override {
    ASSERT_TRUE(InitializeLocalFileSystem(
        kLocalMountPointName, &tmp_dir_, &mount_point_dir_))
        << "Failed to initialize file system.";
  }

  // FileSystemExtensionApiTestBase override.
  void AddTestMountPoint() override {
    EXPECT_TRUE(content::BrowserContext::GetMountPoints(browser()->profile())
                    ->RegisterFileSystem(kLocalMountPointName,
                                         storage::kFileSystemTypeNativeLocal,
                                         storage::FileSystemMountOption(),
                                         mount_point_dir_));
    VolumeManager::Get(browser()->profile())
        ->AddVolumeForTesting(mount_point_dir_, VOLUME_TYPE_TESTING,
                              chromeos::DEVICE_TYPE_UNKNOWN,
                              false /* read_only */);
  }

 private:
  base::ScopedTempDir tmp_dir_;
  base::FilePath mount_point_dir_;
};

// Tests for restricted native local file systems.
class RestrictedFileSystemExtensionApiTest
    : public FileSystemExtensionApiTestBase {
 public:
  RestrictedFileSystemExtensionApiTest() {}
  ~RestrictedFileSystemExtensionApiTest() override {}

  // FileSystemExtensionApiTestBase override.
  void InitTestFileSystem() override {
    ASSERT_TRUE(InitializeLocalFileSystem(
        kRestrictedMountPointName, &tmp_dir_, &mount_point_dir_))
        << "Failed to initialize file system.";
  }

  // FileSystemExtensionApiTestBase override.
  void AddTestMountPoint() override {
    EXPECT_TRUE(
        content::BrowserContext::GetMountPoints(browser()->profile())
            ->RegisterFileSystem(kRestrictedMountPointName,
                                 storage::kFileSystemTypeRestrictedNativeLocal,
                                 storage::FileSystemMountOption(),
                                 mount_point_dir_));
    VolumeManager::Get(browser()->profile())
        ->AddVolumeForTesting(mount_point_dir_, VOLUME_TYPE_TESTING,
                              chromeos::DEVICE_TYPE_UNKNOWN,
                              true /* read_only */);
  }

 private:
  base::ScopedTempDir tmp_dir_;
  base::FilePath mount_point_dir_;
};

// Tests for a drive file system.
class DriveFileSystemExtensionApiTest : public FileSystemExtensionApiTestBase {
 public:
  DriveFileSystemExtensionApiTest() {}
  ~DriveFileSystemExtensionApiTest() override {}

  // FileSystemExtensionApiTestBase override.
  void InitTestFileSystem() override {
    // Set up cache root to be used by DriveIntegrationService. This has to be
    // done before the browser is created because the service instance is
    // initialized by EventRouter.
    ASSERT_TRUE(test_cache_root_.CreateUniqueTempDir());

    // This callback will get called during Profile creation.
    create_drive_integration_service_ = base::Bind(
        &DriveFileSystemExtensionApiTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_.reset(
        new DriveIntegrationServiceFactory::ScopedFactoryForTest(
            &create_drive_integration_service_));
  }

  // FileSystemExtensionApiTestBase override.
  void AddTestMountPoint() override {
    test_util::WaitUntilDriveMountPointIsAdded(browser()->profile());
  }

  // FileSystemExtensionApiTestBase override.
  void TearDown() override {
    FileSystemExtensionApiTestBase::TearDown();
    ui::SelectFileDialog::SetFactory(nullptr);
  }

 protected:
  // DriveIntegrationService factory function for this test.
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    // Ignore signin and lock screen apps profile.
    if (profile->GetPath() == chromeos::ProfileHelper::GetSigninProfileDir() ||
        profile->GetPath() ==
            chromeos::ProfileHelper::GetLockScreenAppProfilePath()) {
      return nullptr;
    }

    // DriveFileSystemExtensionApiTest doesn't expect that several user profiles
    // could exist simultaneously.
    DCHECK(!fake_drive_service_);
    fake_drive_service_ = CreateDriveService();
    return new drive::DriveIntegrationService(
        profile, nullptr, fake_drive_service_, "", test_cache_root_.GetPath(),
        nullptr);
  }

  base::ScopedTempDir test_cache_root_;
  drive::FakeDriveService* fake_drive_service_ = nullptr;
  DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
};

// Tests for Drive file systems in multi-profile setting.
class MultiProfileDriveFileSystemExtensionApiTest :
    public FileSystemExtensionApiTestBase {
 public:
  MultiProfileDriveFileSystemExtensionApiTest() {}

  void SetUpOnMainThread() override {
    base::FilePath user_data_directory;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);
    session_manager::SessionManager::Get()->CreateSession(
        AccountId::FromUserEmail(kSecondProfileAccount), kSecondProfileHash);
    // Set up the secondary profile.
    base::FilePath profile_dir =
        user_data_directory.Append(
            chromeos::ProfileHelper::GetUserProfileDir(
                kSecondProfileHash).BaseName());
    second_profile_ =
        g_browser_process->profile_manager()->GetProfile(profile_dir);

    FileSystemExtensionApiTestBase::SetUpOnMainThread();
  }

  void InitTestFileSystem() override {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());

    // This callback will get called during Profile creation.
    create_drive_integration_service_ = base::Bind(
        &MultiProfileDriveFileSystemExtensionApiTest::
            CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_.reset(
        new DriveIntegrationServiceFactory::ScopedFactoryForTest(
            &create_drive_integration_service_));
  }

  void AddTestMountPoint() override {
    test_util::WaitUntilDriveMountPointIsAdded(browser()->profile());
    test_util::WaitUntilDriveMountPointIsAdded(second_profile_);
  }

 protected:
  // DriveIntegrationService factory function for this test.
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::FilePath cache_dir;
    base::CreateTemporaryDirInDir(tmp_dir_.GetPath(),
                                  base::FilePath::StringType(), &cache_dir);

    drive::FakeDriveService* const service = CreateDriveService();
    return new drive::DriveIntegrationService(
        profile, nullptr, service, std::string(), cache_dir, nullptr);
  }

  void AddTestHostedDocuments() {
    const char kResourceId[] = "unique-id-for-multiprofile-copy-test";
    drive::FakeDriveService* const main_service =
        static_cast<drive::FakeDriveService*>(
            drive::util::GetDriveServiceByProfile(browser()->profile()));
    drive::FakeDriveService* const sub_service =
        static_cast<drive::FakeDriveService*>(
            drive::util::GetDriveServiceByProfile(second_profile_));

    // Place a hosted document under root/test_dir of the sub profile.
    sub_service->AddNewFileWithResourceId(
        kResourceId, "application/vnd.google-apps.document", "", "test_dir",
        "hosted_doc", true, base::Bind(&IgnoreDriveEntryResult));

    // Place the hosted document with no parent in the main profile, for
    // simulating the situation that the document is shared to the main profile.
    main_service->AddNewFileWithResourceId(
        kResourceId, "application/vnd.google-apps.document", "", "",
        "hosted_doc", true, base::Bind(&IgnoreDriveEntryResult));
  }

  base::ScopedTempDir tmp_dir_;
  DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  Profile* second_profile_ = nullptr;
};

class LocalAndDriveFileSystemExtensionApiTest
    : public FileSystemExtensionApiTestBase {
 public:
  LocalAndDriveFileSystemExtensionApiTest() {}
  ~LocalAndDriveFileSystemExtensionApiTest() override {}

  // FileSystemExtensionApiTestBase override.
  void InitTestFileSystem() override {
    ASSERT_TRUE(InitializeLocalFileSystem(
        kLocalMountPointName, &local_tmp_dir_, &local_mount_point_dir_))
        << "Failed to initialize file system.";

    // Set up cache root to be used by DriveIntegrationService. This has to be
    // done before the browser is created because the service instance is
    // initialized by EventRouter.
    ASSERT_TRUE(test_cache_root_.CreateUniqueTempDir());

    // This callback will get called during Profile creation.
    create_drive_integration_service_ = base::Bind(
        &LocalAndDriveFileSystemExtensionApiTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_.reset(
        new DriveIntegrationServiceFactory::ScopedFactoryForTest(
            &create_drive_integration_service_));
  }

  // FileSystemExtensionApiTestBase override.
  void AddTestMountPoint() override {
    EXPECT_TRUE(content::BrowserContext::GetMountPoints(browser()->profile())
                    ->RegisterFileSystem(kLocalMountPointName,
                                         storage::kFileSystemTypeNativeLocal,
                                         storage::FileSystemMountOption(),
                                         local_mount_point_dir_));
    VolumeManager::Get(browser()->profile())
        ->AddVolumeForTesting(local_mount_point_dir_, VOLUME_TYPE_TESTING,
                              chromeos::DEVICE_TYPE_UNKNOWN,
                              false /* read_only */);
    test_util::WaitUntilDriveMountPointIsAdded(browser()->profile());
  }

 protected:
  // DriveIntegrationService factory function for this test.
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    fake_drive_service_ = CreateDriveService();
    return new drive::DriveIntegrationService(
        profile, nullptr, fake_drive_service_, "drive",
        test_cache_root_.GetPath(), nullptr);
  }

 private:
  // For local volume.
  base::ScopedTempDir local_tmp_dir_;
  base::FilePath local_mount_point_dir_;

  // For drive volume.
  base::ScopedTempDir test_cache_root_;
  drive::FakeDriveService* fake_drive_service_ = nullptr;
  DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
};

//
// LocalFileSystemExtensionApiTests.
//

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, FileSystemOperations) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/filesystem_operations_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, FileWatch) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/file_watcher_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, FileBrowserHandlers) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner",
      FILE_PATH_LITERAL("manifest.json"),
      "file_browser/file_browser_handler",
      FLAGS_USE_FILE_HANDLER)) << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest,
                       FileBrowserHandlersLazy) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner",
      FILE_PATH_LITERAL("manifest.json"),
      "file_browser/file_browser_handler_lazy",
      FLAGS_USE_FILE_HANDLER | FLAGS_LAZY_FILE_HANDLER)) << message_;
}

IN_PROC_BROWSER_TEST_F(LocalFileSystemExtensionApiTest, AppFileHandler) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner",
      FILE_PATH_LITERAL("manifest.json"),
      "file_browser/app_file_handler",
      FLAGS_USE_FILE_HANDLER)) << message_;
}

//
// RestrictedFileSystemExtensionApiTests.
//
IN_PROC_BROWSER_TEST_F(RestrictedFileSystemExtensionApiTest,
                       FileSystemOperations) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/filesystem_operations_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

//
// DriveFileSystemExtensionApiTests.
//
#if defined(LEAK_SANITIZER)
#define MAYBE_FileSystemOperations DISABLED_FileSystemOperations
#else
#define MAYBE_FileSystemOperations FileSystemOperations
#endif
IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest,
                       MAYBE_FileSystemOperations) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/filesystem_operations_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, FileWatch) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/file_watcher_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, FileBrowserHandlers) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner",
      FILE_PATH_LITERAL("manifest.json"),
      "file_browser/file_browser_handler",
      FLAGS_USE_FILE_HANDLER)) << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, Search) {
  // Configure the drive service to return only one search result at a time
  // to simulate paginated searches.
  fake_drive_service_->set_default_max_results(1);
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/drive_search_test",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, AppFileHandler) {
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner",
      FILE_PATH_LITERAL("manifest.json"),
      "file_browser/app_file_handler",
      FLAGS_USE_FILE_HANDLER)) << message_;
}

IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, RetainEntry) {
  ui::SelectFileDialog::SetFactory(new FakeSelectFileDialogFactory());
  EXPECT_TRUE(RunFileSystemExtensionApiTest("file_browser/retain_entry",
                                            FILE_PATH_LITERAL("manifest.json"),
                                            "",
                                            FLAGS_NONE))
      << message_;
}

IN_PROC_BROWSER_TEST_F(MultiProfileDriveFileSystemExtensionApiTest,
                       CrossProfileCopy) {
  AddTestHostedDocuments();
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/multi_profile_copy",
      FILE_PATH_LITERAL("manifest.json"),
      "",
      FLAGS_NONE)) << message_;
}

//
// LocalAndDriveFileSystemExtensionApiTests.
//
IN_PROC_BROWSER_TEST_F(LocalAndDriveFileSystemExtensionApiTest,
                       AppFileHandlerMulti) {
  EXPECT_TRUE(
      RunFileSystemExtensionApiTest("file_browser/app_file_handler_multi",
                                    FILE_PATH_LITERAL("manifest.json"),
                                    "",
                                    FLAGS_NONE))
      << message_;
}
}  // namespace
}  // namespace file_manager
