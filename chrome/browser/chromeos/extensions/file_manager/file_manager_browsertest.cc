// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Browser test for basic Chrome OS file manager functionality:
//  - The file list is updated when a file is added externally to the Downloads
//    folder.
//  - Selecting a file and copy-pasting it with the keyboard copies the file.
//  - Selecting a file and pressing delete deletes it.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/time.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/extensions/file_manager/drive_test_util.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_context.h"
#include "webkit/browser/fileapi/external_mount_points.h"

namespace {

// Test suffixes appended to the Javascript tests' names.
const char kDownloadsVolume[] = "Downloads";
const char kDriveVolume[] = "Drive";

enum EntryType {
  FILE,
  DIRECTORY,
};

enum SharedOption {
  NONE,
  SHARED,
};

struct TestEntryInfo {
  EntryType type;
  const char* source_file_name;  // Source file name to be used as a prototype.
  const char* target_name;  // Target file or directory name.
  const char* mime_type;
  SharedOption shared_option;
  const char* last_modified_time_as_string;
};

TestEntryInfo kTestEntrySetCommon[] = {
  { FILE, "text.txt", "hello.txt", "text/plain", NONE, "4 Sep 1998 12:34:56" },
  { FILE, "image.png", "My Desktop Background.png", "text/plain", NONE,
    "18 Jan 2038 01:02:03" },
  { FILE, "music.ogg", "Beautiful Song.ogg", "text/plain", NONE,
    "12 Nov 2086 12:00:00" },
  { FILE, "video.ogv", "world.ogv", "text/plain", NONE,
    "4 July 2012 10:35:00" },
  { DIRECTORY, "", "photos", NULL, NONE, "1 Jan 1980 23:59:59" },
  { DIRECTORY, "", ".warez", NULL, NONE, "26 Oct 1985 13:39" }
};

TestEntryInfo kTestEntrySetDriveOnly[] = {
  { FILE, "", "Test Document", "application/vnd.google-apps.document", NONE,
    "10 Apr 2013 16:20:00" },
  { FILE, "", "Test Shared Document", "application/vnd.google-apps.document",
    SHARED, "20 Mar 2013 22:40:00" }
};

// The base class of volumes for test.
// Sub-classes of this class are used by test cases and provide operations such
// as creating files for each type of test volume.
class TestVolume {
 public:
  virtual ~TestVolume() {}

  // Creates an entry with given information.
  virtual void CreateEntry(const TestEntryInfo& entry) = 0;

  // Returns the name of volume such as "Downloads" or "Drive".
  virtual std::string GetName() const = 0;
};

// The local volume class for test.
// This class provides the operations for a test volume that simulates local
// drive.
class LocalTestVolume : public TestVolume {
 public:
  explicit LocalTestVolume(const std::string& mount_name)
      : mount_name_(mount_name) {
  }

  // Adds this volume to the file system as a local volume. Returns true on
  // success.
  bool Mount(Profile* profile) {
    if (local_path_.empty()) {
      if (!tmp_dir_.CreateUniqueTempDir())
        return false;
      local_path_ = tmp_dir_.path().Append(mount_name_);
    }
    fileapi::ExternalMountPoints* const mount_points =
        content::BrowserContext::GetMountPoints(profile);
    mount_points->RevokeFileSystem(mount_name_);
    if (!mount_points->RegisterFileSystem(
            mount_name_,
            fileapi::kFileSystemTypeNativeLocal,
            local_path_)) {
      return false;
    }
    if (!file_util::CreateDirectory(local_path_))
      return false;
    return true;
  }

  virtual void CreateEntry(const TestEntryInfo& entry) OVERRIDE {
    if (entry.type == DIRECTORY) {
      CreateDirectory(entry.target_name ,
                      entry.last_modified_time_as_string);
    } else if (entry.type == FILE) {
      CreateFile(entry.source_file_name, entry.target_name,
                 entry.last_modified_time_as_string);
    } else {
      NOTREACHED();
    }
  }

  void CreateFile(const std::string& source_file_name,
                  const std::string& target_name,
                  const std::string& modification_time) {
    std::string content_data;
    base::FilePath test_file_path =
        google_apis::test_util::GetTestFilePath("chromeos/file_manager").
            AppendASCII(source_file_name);

    base::FilePath path = local_path_.AppendASCII(target_name);
    ASSERT_TRUE(file_util::PathExists(test_file_path))
        << "Test file doesn't exist: " << test_file_path.value();
    ASSERT_TRUE(file_util::CopyFile(test_file_path, path));
    ASSERT_TRUE(file_util::PathExists(path))
        << "Copying to: " << path.value() << " failed.";
    base::Time time;
    ASSERT_TRUE(base::Time::FromString(modification_time.c_str(), &time));
    ASSERT_TRUE(file_util::SetLastModifiedTime(path, time));
  }

  void CreateDirectory(const std::string& target_name,
                       const std::string& modification_time) {
    base::FilePath path = local_path_.AppendASCII(target_name);
    ASSERT_TRUE(file_util::CreateDirectory(path)) <<
        "Failed to create a directory: " << target_name;
    base::Time time;
    ASSERT_TRUE(base::Time::FromString(modification_time.c_str(), &time));
    ASSERT_TRUE(file_util::SetLastModifiedTime(path, time));
  }

  virtual std::string GetName() const OVERRIDE {
    return mount_name_;
  }

 private:
  std::string mount_name_;
  base::FilePath local_path_;
  base::ScopedTempDir tmp_dir_;
};

// The drive volume class for test.
// This class provides the operations for a test volume that simulates Google
// drive.
class DriveTestVolume : public TestVolume {
 public:
  DriveTestVolume() : fake_drive_service_(NULL),
                      integration_service_(NULL) {
  }

  // Send request to add this volume to the file system as Google drive.
  // This method must be calld at SetUp method of FileManagerBrowserTestBase.
  // Returns true on success.
  bool SetUp() {
    if (!test_cache_root_.CreateUniqueTempDir())
      return false;
    drive::DriveIntegrationServiceFactory::SetFactoryForTest(
        base::Bind(&DriveTestVolume::CreateDriveIntegrationService,
                   base::Unretained(this)));
    return true;
  }

  virtual void CreateEntry(const TestEntryInfo& entry) OVERRIDE {
    if (entry.type == DIRECTORY) {
      CreateDirectory(entry.target_name,
                      entry.last_modified_time_as_string);
    } else if (entry.type == FILE) {
      CreateFile(entry.source_file_name,
                 entry.target_name,
                 entry.mime_type,
                 entry.shared_option == SHARED,
                 entry.last_modified_time_as_string);
    } else {
      NOTREACHED();
    }
  }

  // Creates an empty directory with the given |name| and |modification_time|.
  void CreateDirectory(const std::string& name,
                       const std::string& modification_time) {
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    scoped_ptr<google_apis::ResourceEntry> resource_entry;
    fake_drive_service_->AddNewDirectory(
        fake_drive_service_->GetRootResourceId(),
        name,
        google_apis::test_util::CreateCopyResultCallback(&error,
                                                         &resource_entry));
    base::MessageLoop::current()->RunUntilIdle();
    ASSERT_TRUE(error == google_apis::HTTP_CREATED);
    ASSERT_TRUE(resource_entry);

    base::Time time;
    ASSERT_TRUE(base::Time::FromString(modification_time.c_str(), &time));
    fake_drive_service_->SetLastModifiedTime(
        resource_entry->resource_id(),
        time,
        google_apis::test_util::CreateCopyResultCallback(&error,
                                                         &resource_entry));
    base::MessageLoop::current()->RunUntilIdle();
    ASSERT_TRUE(error == google_apis::HTTP_SUCCESS);
    ASSERT_TRUE(resource_entry);
    CheckForUpdates();
  }

  virtual std::string GetName() const OVERRIDE {
    return "Drive";
  }

  // Creates a test file with the given spec.
  // Serves |test_file_name| file. Pass an empty string for an empty file.
  void CreateFile(const std::string& source_file_name,
                  const std::string& target_file_name,
                  const std::string& mime_type,
                  bool shared_with_me,
                  const std::string& modification_time) {
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

    std::string content_data;
    if (!source_file_name.empty()) {
      base::FilePath source_file_path =
          google_apis::test_util::GetTestFilePath("chromeos/file_manager").
              AppendASCII(source_file_name);
      ASSERT_TRUE(file_util::ReadFileToString(source_file_path, &content_data));
    }

    scoped_ptr<google_apis::ResourceEntry> resource_entry;
    fake_drive_service_->AddNewFile(
        mime_type,
        content_data,
        fake_drive_service_->GetRootResourceId(),
        target_file_name,
        shared_with_me,
        google_apis::test_util::CreateCopyResultCallback(&error,
                                                         &resource_entry));
    base::MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
    ASSERT_TRUE(resource_entry);

    base::Time time;
    ASSERT_TRUE(base::Time::FromString(modification_time.c_str(), &time));
    fake_drive_service_->SetLastModifiedTime(
        resource_entry->resource_id(),
        time,
        google_apis::test_util::CreateCopyResultCallback(&error,
                                                         &resource_entry));
    base::MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
    ASSERT_TRUE(resource_entry);

    CheckForUpdates();
  }

  // Notifies FileSystem that the contents in FakeDriveService are
  // changed, hence the new contents should be fetched.
  void CheckForUpdates() {
    if (integration_service_ && integration_service_->file_system()) {
      integration_service_->file_system()->CheckForUpdates();
    }
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    fake_drive_service_ = new google_apis::FakeDriveService;
    fake_drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/empty_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi("chromeos/drive/applist.json");
    integration_service_ = new drive::DriveIntegrationService(
        profile,
        fake_drive_service_,
        test_cache_root_.path(),
        NULL);
    return integration_service_;
  }

 private:
  base::ScopedTempDir test_cache_root_;
  google_apis::FakeDriveService* fake_drive_service_;
  drive::DriveIntegrationService* integration_service_;
};

// The base test class. Used by FileManagerBrowserLocalTest,
// FileManagerBrowserDriveTest, and FileManagerBrowserTransferTest.
// The boolean parameter, retrieved by GetParam(), is true if testing in the
// guest mode. See SetUpCommandLine() below for details.
class FileManagerBrowserTestBase : public ExtensionApiTest,
                                   public ::testing::WithParamInterface<bool> {
 protected:
  // Adds an incognito and guest-mode flags for tests in the guest mode.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  // Loads our testing extension and sends it a string identifying the current
  // test.
  void StartTest(const std::string& test_name);

  // Creates test files and directories.
  void CreateTestEntries(TestVolume* volume, const TestEntryInfo* entries,
                         size_t num_entries);

  // Runs the file display test on the passed |volume|, shared by subclasses.
  void DoTestFileDisplay(TestVolume* volume);
};

void FileManagerBrowserTestBase::SetUpCommandLine(CommandLine* command_line) {
  bool in_guest_mode = GetParam();
  if (in_guest_mode) {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitchNative(chromeos::switches::kLoginUser, "");
    command_line->AppendSwitch(switches::kIncognito);
  }
  ExtensionApiTest::SetUpCommandLine(command_line);
}

void FileManagerBrowserTestBase::StartTest(const std::string& test_name) {
  base::FilePath path = test_data_dir_.AppendASCII("file_manager_browsertest");
  const extensions::Extension* extension = LoadExtensionAsComponent(path);
  ASSERT_TRUE(extension);

  bool in_guest_mode = GetParam();
  ExtensionTestMessageListener listener(
      in_guest_mode ? "which test guest" : "which test non-guest", true);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(test_name);
}

void FileManagerBrowserTestBase::CreateTestEntries(
    TestVolume* volume, const TestEntryInfo* entries, size_t num_entries) {
  for (size_t i = 0; i < num_entries; ++i) {
    volume->CreateEntry(entries[i]);
  }
}

void FileManagerBrowserTestBase::DoTestFileDisplay(TestVolume* volume) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("fileDisplay" + volume->GetName()));

  ExtensionTestMessageListener listener("initial check done", true);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  const TestEntryInfo entry = {
    FILE,
    "music.ogg",  // Prototype file name.
    "newly added file.ogg",  // Target file name.
    "audio/ogg",
    NONE,
    "4 Sep 1998 00:00:00"
  };
  volume->CreateEntry(entry);
  listener.Reply("file added");

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// A class to test local volumes.
class FileManagerBrowserLocalTest : public FileManagerBrowserTestBase {
 public:
  FileManagerBrowserLocalTest() : volume_("Downloads") {}

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    FileManagerBrowserTestBase::SetUpInProcessBrowserTestFixture();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    FileManagerBrowserTestBase::SetUpOnMainThread();
    ASSERT_TRUE(volume_.Mount(browser()->profile()));
    CreateTestEntries(&volume_, kTestEntrySetCommon,
                      arraysize(kTestEntrySetCommon));
  }

  LocalTestVolume volume_;
};

INSTANTIATE_TEST_CASE_P(InGuestMode,
                        FileManagerBrowserLocalTest,
                        ::testing::Values(true));

INSTANTIATE_TEST_CASE_P(InNonGuestMode,
                        FileManagerBrowserLocalTest,
                        ::testing::Values(false));

// A class to test Drive's volumes
class FileManagerBrowserDriveTest : public FileManagerBrowserTestBase {
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    FileManagerBrowserTestBase::SetUpInProcessBrowserTestFixture();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    ASSERT_TRUE(volume_.SetUp());
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    FileManagerBrowserTestBase::SetUpOnMainThread();
    CreateTestEntries(&volume_, kTestEntrySetCommon,
                      arraysize(kTestEntrySetCommon));
    // For testing Drive, create more entries with Drive specific attributes.
    // TODO(haruki): Add a case for an entry cached by DriveCache.
    CreateTestEntries(&volume_, kTestEntrySetDriveOnly,
                      arraysize(kTestEntrySetDriveOnly));
    drive_test_util::WaitUntilDriveMountPointIsAdded(browser()->profile());
  }

  DriveTestVolume volume_;
};

// Don't test Drive in the guest mode as it's not supported.
INSTANTIATE_TEST_CASE_P(InNonGuestMode,
                        FileManagerBrowserDriveTest,
                        ::testing::Values(false));

// A class to test both local and Drive's volumes.
class FileManagerBrowserTransferTest : public FileManagerBrowserTestBase {
 public:
  FileManagerBrowserTransferTest() : local_volume_("Downloads") {}

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    FileManagerBrowserTestBase::SetUpInProcessBrowserTestFixture();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    ASSERT_TRUE(drive_volume_.SetUp());
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    FileManagerBrowserTestBase::SetUpOnMainThread();
    ASSERT_TRUE(local_volume_.Mount(browser()->profile()));
    CreateTestEntries(&local_volume_, kTestEntrySetCommon,
                      arraysize(kTestEntrySetCommon));
    CreateTestEntries(&drive_volume_, kTestEntrySetCommon,
                      arraysize(kTestEntrySetCommon));
    CreateTestEntries(&drive_volume_, kTestEntrySetDriveOnly,
                      arraysize(kTestEntrySetDriveOnly));
    drive_test_util::WaitUntilDriveMountPointIsAdded(browser()->profile());
  }

  LocalTestVolume local_volume_;
  DriveTestVolume drive_volume_;
};

// FileManagerBrowserTransferTest depends on Drive and Drive is not supported in
// the guest mode.
INSTANTIATE_TEST_CASE_P(InNonGuestMode,
                        FileManagerBrowserTransferTest,
                        ::testing::Values(false));

IN_PROC_BROWSER_TEST_P(FileManagerBrowserLocalTest, TestFileDisplay) {
  DoTestFileDisplay(&volume_);
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserLocalTest, TestGalleryOpen) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("galleryOpenDownloads"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Disabled temporarily since fails on Linux Chromium OS ASAN Tests (2).
// TODO(mtomasz): crbug.com/243611.
IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, DISABLED_TestGalleryOpen) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("galleryOpenDrive"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserLocalTest, TestAudioOpen) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("galleryOpenDownloads"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestAudioOpen) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("galleryOpenDrive"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserLocalTest, TestVideoOpen) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("galleryOpenDownloads"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestVideoOpen) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("galleryOpenDrive"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestKeyboardCopy) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("keyboardCopyDrive"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestKeyboardDelete) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("keyboardDeleteDrive"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestOpenRecent) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("openSidebarRecent"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestOpenOffline) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("openSidebarOffline"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestOpenSharedWithMe) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("openSidebarSharedWithMe"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestAutocomplete) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("autocomplete"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromDriveToDownloads) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("transferFromDriveToDownloads"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromDownloadsToDrive) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("transferFromDownloadsToDrive"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromSharedToDownloads) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("transferFromSharedToDownloads"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromSharedToDrive) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("transferFromSharedToDrive"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromRecentToDownloads) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("transferFromRecentToDownloads"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromRecentToDrive) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("transferFromRecentToDrive"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromOfflineToDownloads) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("transferFromOfflineToDownloads"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromOfflineToDrive) {
  ResultCatcher catcher;
  ASSERT_NO_FATAL_FAILURE(StartTest("transferFromOfflineToDrive"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace
