// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_manager/drive_test_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "webkit/browser/fileapi/external_mount_points.h"

// Tests for access to external file systems (as defined in
// webkit/common/fileapi/file_system_types.h) from extensions with
// fileBrowserPrivate and fileBrowserHandler extension permissions.
// The tests cover following external file system types:
// - local (kFileSystemTypeLocalNative): a local file system on which files are
//   accessed using native local path.
// - restricted (kFileSystemTypeRestrictedLocalNative): a *read-only* local file
//   system which can only be accessed by extensions that have full access to
//   external file systems (i.e. extensions with fileBrowserPrivate permission).
// - drive (kFileSystemTypeDrive): a file system that provides access to Google
//   Drive.
//
// The tests cover following scenarios:
// - Performing file system operations on external file systems from an
//   extension with fileBrowserPrivate permission (i.e. a file browser
//   extension).
// - Performing read/write operations from file handler extensions. These
//   extensions need a file browser extension to give them permissions to access
//   files. This also includes file handler extensions in filesystem API.
// - Observing directory changes from a file browser extension (using
//   fileBrowserPrivate API).
// - Doing searches on drive file system from file browser extension (using
//   fileBrowserPrivate API).

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

// Contains feed for drive file system. The file system hierarchy is the same
// for local and restricted file systems:
//   test_dir/ - - subdir/
//              |
//               - empty_test_dir/
//              |
//               - empty_test_file.foo
//              |
//               - test_file.xul
//              |
//               - test_file.xul.foo
//              |
//               - test_file.tiff
//              |
//               - test_file.tiff.foo
//
// All files except test_dir/empty_file.foo, which is empty, initially contain
// kTestFileContent.
const char kTestRootFeed[] =
    "gdata/remote_file_system_apitest_root_feed.json";

// Sets up the initial file system state for native local and restricted native
// local file systems. The hierarchy is the same as for the drive file system.
bool InitializeLocalFileSystem(base::ScopedTempDir* tmp_dir,
                               base::FilePath* mount_point_dir) {
  if (!tmp_dir->CreateUniqueTempDir())
    return false;

  *mount_point_dir = tmp_dir->path().AppendASCII("mount");
  // Create the mount point.
  if (!file_util::CreateDirectory(*mount_point_dir))
    return false;

  base::FilePath test_dir = mount_point_dir->AppendASCII("test_dir");
  if (!file_util::CreateDirectory(test_dir))
    return false;

  base::FilePath test_subdir = test_dir.AppendASCII("empty_test_dir");
  if (!file_util::CreateDirectory(test_subdir))
    return false;

  test_subdir = test_dir.AppendASCII("subdir");
  if (!file_util::CreateDirectory(test_subdir))
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

// Helper class to wait for a background page to load or close again.
class BackgroundObserver {
 public:
  BackgroundObserver()
      : page_created_(chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                      content::NotificationService::AllSources()),
        page_closed_(chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                     content::NotificationService::AllSources()) {
  }

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
  virtual ~FileSystemExtensionApiTestBase() {}

  virtual void SetUp() OVERRIDE {
    InitTestFileSystem();
    ExtensionApiTest::SetUp();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    AddTestMountPoint();
    ExtensionApiTest::SetUpOnMainThread();
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

    ResultCatcher catcher;

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
  // Sets up initial test file system hierarchy. See comment for kTestRootFeed
  // for the actual hierarchy.
  virtual void InitTestFileSystem() = 0;
  // Registers mount point used in the test.
  virtual void AddTestMountPoint() = 0;
};

// Tests for a native local file system.
class LocalFileSystemExtensionApiTest : public FileSystemExtensionApiTestBase {
 public:
  LocalFileSystemExtensionApiTest() {}
  virtual ~LocalFileSystemExtensionApiTest() {}

  // FileSystemExtensionApiTestBase OVERRIDE.
  virtual void InitTestFileSystem() OVERRIDE {
    ASSERT_TRUE(InitializeLocalFileSystem(&tmp_dir_, &mount_point_dir_))
        << "Failed to initialize file system.";
  }

  // FileSystemExtensionApiTestBase OVERRIDE.
  virtual void AddTestMountPoint() OVERRIDE {
    EXPECT_TRUE(content::BrowserContext::GetMountPoints(browser()->profile())->
        RegisterFileSystem(kLocalMountPointName,
                           fileapi::kFileSystemTypeNativeLocal,
                           mount_point_dir_));
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
  virtual ~RestrictedFileSystemExtensionApiTest() {}

  // FileSystemExtensionApiTestBase OVERRIDE.
  virtual void InitTestFileSystem() OVERRIDE {
    ASSERT_TRUE(InitializeLocalFileSystem(&tmp_dir_, &mount_point_dir_))
        << "Failed to initialize file system.";
  }

  // FileSystemExtensionApiTestBase OVERRIDE.
  virtual void AddTestMountPoint() OVERRIDE {
    EXPECT_TRUE(content::BrowserContext::GetMountPoints(browser()->profile())->
        RegisterFileSystem(kRestrictedMountPointName,
                           fileapi::kFileSystemTypeRestrictedNativeLocal,
                           mount_point_dir_));
  }

 private:
  base::ScopedTempDir tmp_dir_;
  base::FilePath mount_point_dir_;
};

// Tests for a drive file system.
class DriveFileSystemExtensionApiTest : public FileSystemExtensionApiTestBase {
 public:
  DriveFileSystemExtensionApiTest() : fake_drive_service_(NULL) {}
  virtual ~DriveFileSystemExtensionApiTest() {}

  // FileSystemExtensionApiTestBase OVERRIDE.
  virtual void InitTestFileSystem() OVERRIDE {
    // Set up cache root to be used by DriveIntegrationService. This has to be
    // done before the browser is created because the service instance is
    // initialized by EventRouter.
    ASSERT_TRUE(test_cache_root_.CreateUniqueTempDir());

    drive::DriveIntegrationServiceFactory::SetFactoryForTest(
        base::Bind(
            &DriveFileSystemExtensionApiTest::CreateDriveIntegrationService,
            base::Unretained(this)));
  }

  // FileSystemExtensionApiTestBase OVERRIDE.
  virtual void AddTestMountPoint() OVERRIDE {
    test_util::WaitUntilDriveMountPointIsAdded(browser()->profile());
  }

 protected:
  // DriveIntegrationService factory function for this test.
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    fake_drive_service_ = new drive::FakeDriveService;
    fake_drive_service_->LoadResourceListForWapi(kTestRootFeed);
    fake_drive_service_->LoadAccountMetadataForWapi(
        "gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi("drive/applist.json");

    return new drive::DriveIntegrationService(profile,
                                              fake_drive_service_,
                                              test_cache_root_.path(),
                                              NULL);
  }

  base::ScopedTempDir test_cache_root_;
  drive::FakeDriveService* fake_drive_service_;
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
IN_PROC_BROWSER_TEST_F(DriveFileSystemExtensionApiTest, FileSystemOperations) {
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
  fake_drive_service_->set_default_max_results(1);
  EXPECT_TRUE(RunFileSystemExtensionApiTest(
      "file_browser/handler_test_runner",
      FILE_PATH_LITERAL("manifest.json"),
      "file_browser/app_file_handler",
      FLAGS_USE_FILE_HANDLER)) << message_;
}

}  // namespace
}  // namespace file_manager
