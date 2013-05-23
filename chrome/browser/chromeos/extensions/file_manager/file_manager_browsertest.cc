// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Browser test for basic Chrome OS file manager functionality:
//  - The file list is updated when a file is added externally to the Downloads
//    folder.
//  - Selecting a file and copy-pasting it with the keyboard copies the file.
//  - Selecting a file and pressing delete deletes it.

#include <algorithm>
#include <string>

#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/platform_file.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system.h"
#include "chrome/browser/chromeos/drive/file_system_observer.h"
#include "chrome/browser/chromeos/extensions/file_manager/drive_test_util.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "net/base/escape.h"
#include "webkit/fileapi/external_mount_points.h"

namespace {

const char kKeyboardTestFileName[] = "world.ogv";
const char kKeyboardTestFileCopyName[] = "world (1).ogv";

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
  { FILE, "video.ogv", kKeyboardTestFileName, "text/plain", NONE,
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

// Monitors changes to a single file until the supplied condition callback
// returns true. Usage:
//   TestFilePathWatcher watcher(path_to_file, MyConditionCallback);
//   watcher.StartAndWaitUntilReady();
//   ... trigger filesystem modification ...
//   watcher.RunMessageLoopUntilConditionSatisfied();
class TestFilePathWatcher {
 public:
  typedef base::Callback<bool(const base::FilePath& file_path)>
      ConditionCallback;

  // Stores the supplied |path| and |condition| for later use (no side effects).
  TestFilePathWatcher(const base::FilePath& path,
                      const ConditionCallback& condition);

  // Waits (running a message pump) until the callback returns true or
  // FilePathWatcher reports an error. Return true on success.
  bool RunMessageLoopUntilConditionSatisfied();

 private:
  // Starts the FilePathWatcher to watch the target file. Also check if the
  // condition is already met.
  void StartWatching();

  // FilePathWatcher callback (on the FILE thread). Posts Done() to the UI
  // thread when the condition is satisfied or there is an error.
  void FilePathWatcherCallback(const base::FilePath& path, bool error);

  const base::FilePath path_;
  ConditionCallback condition_;
  scoped_ptr<base::FilePathWatcher> watcher_;
  base::RunLoop run_loop_;
  base::Closure quit_closure_;
  bool failed_;
};

TestFilePathWatcher::TestFilePathWatcher(const base::FilePath& path,
                                         const ConditionCallback& condition)
    : path_(path),
      condition_(condition),
      quit_closure_(run_loop_.QuitClosure()),
      failed_(false) {
}

void TestFilePathWatcher::StartWatching() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  watcher_.reset(new base::FilePathWatcher);
  bool ok = watcher_->Watch(
      path_, false /*recursive*/,
      base::Bind(&TestFilePathWatcher::FilePathWatcherCallback,
                 base::Unretained(this)));
  ASSERT_TRUE(ok);

  // If the condition was already met before FilePathWatcher was launched,
  // FilePathWatcher won't be able to detect a change, so check the condition
  // here.
  if (condition_.Run(path_)) {
    watcher_.reset();
    content::BrowserThread::PostTask(content::BrowserThread::UI,
                                     FROM_HERE,
                                     quit_closure_);
    return;
  }
}

void TestFilePathWatcher::FilePathWatcherCallback(const base::FilePath& path,
                                                  bool failed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  DCHECK_EQ(path_.value(), path.value());

  if (failed || condition_.Run(path)) {
    failed_ = failed;
    watcher_.reset();
    content::BrowserThread::PostTask(content::BrowserThread::UI,
                                     FROM_HERE,
                                     quit_closure_);
  }
}

bool TestFilePathWatcher::RunMessageLoopUntilConditionSatisfied() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&TestFilePathWatcher::StartWatching,
                 base::Unretained(this)));

  // Wait until the condition is met.
  run_loop_.Run();
  return !failed_;
}

// Returns true if a file with the given size is present at |path|.
bool FilePresentWithSize(const int64 file_size,
                         const base::FilePath& path) {
  int64 copy_size = 0;
  // If the file doesn't exist yet this will fail and we'll keep waiting.
  if (!file_util::GetFileSize(path, &copy_size))
    return false;
  return (copy_size == file_size);
}

// Returns true if a file is not present at |path|.
bool FileNotPresent(const base::FilePath& path) {
  return !file_util::PathExists(path);
};

// The base class of volumes for test.
// Sub-classes of this class are used by test cases and provide operations such
// as creating files for each type of test volume.
class TestVolume {
 public:
  virtual ~TestVolume() {}

  // Creates an entry with given information.
  virtual void CreateEntry(const TestEntryInfo& entry) = 0;

  // Returns the path of the root directory.
  virtual base::FilePath GetRootPath() const = 0;

  // Returns true if |file_path| exists.
  virtual bool PathExists(const base::FilePath& file_path) const = 0;

  // Returns the name of volume such as "Downloads" or "Drive".
  virtual std::string GetName() const = 0;

  // Waits until a file with the given size is present at |path|. Returns
  // true on success.
  virtual bool WaitUntilFilePresentWithSize(const base::FilePath& file_path,
                                            int64 file_size) = 0;

  // Waits until a file is not present at |path|. Returns true on success.
  virtual bool WaitUntilFileNotPresent(const base::FilePath& file_path) = 0;
};

// The local volume class for test.
// This class provides the operations for a test volume that simulates local
// drive.
class LocalTestVolume : public TestVolume {
 public:
  explicit LocalTestVolume(const std::string& mount_name)
      : mount_name_(mount_name) {
  }

  LocalTestVolume(const std::string& mount_name,
                  const base::FilePath& local_path)
      : mount_name_(mount_name),
        local_path_(local_path) {
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

  virtual base::FilePath GetRootPath() const OVERRIDE {
    return local_path_;
  }

  virtual bool PathExists(const base::FilePath& file_path) const OVERRIDE {
    return file_util::PathExists(file_path);
  }

  virtual bool WaitUntilFilePresentWithSize(
      const base::FilePath& file_path,
      int64 file_size) OVERRIDE {
    TestFilePathWatcher watcher(file_path, base::Bind(FilePresentWithSize,
                                                      file_size));
    return watcher.RunMessageLoopUntilConditionSatisfied();
  }

  virtual bool WaitUntilFileNotPresent(
      const base::FilePath& file_path) OVERRIDE {
    TestFilePathWatcher watcher(file_path, base::Bind(FileNotPresent));
    return watcher.RunMessageLoopUntilConditionSatisfied();
  }

 private:
  std::string mount_name_;
  base::FilePath local_path_;
  base::ScopedTempDir tmp_dir_;
};

// The drive volume class for test.
// This class provides the operations for a test volume that simulates Google
// drive.
class DriveTestVolume : public TestVolume,
                        public drive::FileSystemObserver {
 public:
  DriveTestVolume() : fake_drive_service_(NULL),
                      integration_service_(NULL),
                      waiting_for_directory_change_(false) {
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
    MessageLoop::current()->RunUntilIdle();
    ASSERT_TRUE(error == google_apis::HTTP_CREATED);
    ASSERT_TRUE(resource_entry);

    base::Time time;
    ASSERT_TRUE(base::Time::FromString(modification_time.c_str(), &time));
    fake_drive_service_->SetLastModifiedTime(
        resource_entry->resource_id(),
        time,
        google_apis::test_util::CreateCopyResultCallback(&error,
                                                         &resource_entry));
    MessageLoop::current()->RunUntilIdle();
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
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
    ASSERT_TRUE(resource_entry);

    base::Time time;
    ASSERT_TRUE(base::Time::FromString(modification_time.c_str(), &time));
    fake_drive_service_->SetLastModifiedTime(
        resource_entry->resource_id(),
        time,
        google_apis::test_util::CreateCopyResultCallback(&error,
                                                         &resource_entry));
    MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
    ASSERT_TRUE(resource_entry);

    CheckForUpdates();
  }

  virtual base::FilePath GetRootPath() const OVERRIDE {
    return base::FilePath(drive::util::kDriveMyDriveRootPath);
  }

  virtual bool PathExists(const base::FilePath& file_path) const OVERRIDE {
    DCHECK(integration_service_);
    DCHECK(integration_service_->file_system());

    drive::FileError error = drive::FILE_ERROR_FAILED;
    scoped_ptr<drive::ResourceEntry> entry_proto;
    integration_service_->file_system()->GetResourceEntryByPath(
        file_path,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry_proto));
    google_apis::test_util::RunBlockingPoolTask();

    return error == drive::FILE_ERROR_OK;
  }

  virtual bool WaitUntilFilePresentWithSize(
      const base::FilePath& file_path,
      int64 file_size) OVERRIDE {
    while (true) {
      if (FileIsPresentWithSize(file_path, file_size))
        return true;
      WaitUntilDirectoryChanged();
    }
    NOTREACHED();
    return false;
  }

  virtual bool WaitUntilFileNotPresent(
      const base::FilePath& file_path) OVERRIDE {
    while (true) {
      if (FileIsNotPresent(file_path))
        return true;
      WaitUntilDirectoryChanged();
    }
    NOTREACHED();
    return false;
  }

  virtual void OnDirectoryChanged(
      const base::FilePath& directory_path) OVERRIDE {
    if (waiting_for_directory_change_)
      MessageLoop::current()->Quit();
  }

  // Notifies FileSystem that the contents in FakeDriveService are
  // changed, hence the new contents should be fetched.
  void CheckForUpdates() {
    if (integration_service_ && integration_service_->file_system()) {
      integration_service_->file_system()->CheckForUpdates();
    }
  }

  // Waits until a notification for a directory change is received.
  void WaitUntilDirectoryChanged() {
    waiting_for_directory_change_ = true;
    MessageLoop::current()->Run();
    waiting_for_directory_change_ = false;
  }

  // Returns true if a file of the size |file_size| is present at |file_path|.
  bool FileIsPresentWithSize(
      const base::FilePath& file_path,
      int64 file_size) {
    DCHECK(integration_service_);
    DCHECK(integration_service_->file_system());

    drive::FileError error = drive::FILE_ERROR_FAILED;
    scoped_ptr<drive::ResourceEntry> entry_proto;
    integration_service_->file_system()->GetResourceEntryByPath(
        file_path,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry_proto));
    google_apis::test_util::RunBlockingPoolTask();

    return (error == drive::FILE_ERROR_OK &&
            entry_proto->file_info().size() == file_size);
  }

  // Returns true if a file is not present at |file_path|.
  bool FileIsNotPresent(
      const base::FilePath& file_path) {
    DCHECK(integration_service_);
    DCHECK(integration_service_->file_system());

    drive::FileError error = drive::FILE_ERROR_FAILED;
    scoped_ptr<drive::ResourceEntry> entry_proto;
    integration_service_->file_system()->GetResourceEntryByPath(
        file_path,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry_proto));
    google_apis::test_util::RunBlockingPoolTask();

    return error == drive::FILE_ERROR_NOT_FOUND;
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
    integration_service_->file_system()->AddObserver(this);
    return integration_service_;
  }

 private:
  base::ScopedTempDir test_cache_root_;
  google_apis::FakeDriveService* fake_drive_service_;
  drive::DriveIntegrationService* integration_service_;
  bool waiting_for_directory_change_;
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

  // After starting the test, set up volumes.
  virtual void PrepareVolume() = 0;

  // Runs the file display test on the passed |volume|, shared by subclasses.
  void DoTestFileDisplay(TestVolume* volume);

  // Runs the keyboard copy test on the passed |volume|, shared by subclasses.
  void DoTestKeyboardCopy(TestVolume* volume);

  // Runs the keyboard delete test on the passed |volume|, shared by subclasses.
  void DoTestKeyboardDelete(TestVolume* volume);
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
  StartTest("fileDisplay" + volume->GetName());

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

void FileManagerBrowserTestBase::DoTestKeyboardCopy(TestVolume* volume) {
  base::FilePath copy_path =
      volume->GetRootPath().AppendASCII(kKeyboardTestFileCopyName);
  ASSERT_FALSE(volume->PathExists(copy_path));

  ResultCatcher catcher;
  StartTest("keyboardCopy" + volume->GetName());

  const int64 kKeyboardTestFileSize = 59943;

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(volume->WaitUntilFilePresentWithSize(
      copy_path, kKeyboardTestFileSize));

  // Check that it was a copy, not a move.
  base::FilePath source_path =
      volume->GetRootPath().AppendASCII(kKeyboardTestFileName);
  ASSERT_TRUE(volume->PathExists(source_path));
}

void FileManagerBrowserTestBase::DoTestKeyboardDelete(TestVolume* volume) {
  base::FilePath delete_path =
      volume->GetRootPath().AppendASCII(kKeyboardTestFileName);
  ASSERT_TRUE(volume->PathExists(delete_path));

  ResultCatcher catcher;
  StartTest("keyboardDelete" + volume->GetName());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(volume->WaitUntilFileNotPresent(delete_path));
}

// A class to test local volumes.
class FileManagerBrowserLocalTest : public FileManagerBrowserTestBase {
 public:
  FileManagerBrowserLocalTest() : volume_("Downloads") {}

  virtual void SetUp() OVERRIDE {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    ExtensionApiTest::SetUp();
  }

 protected:
  virtual void PrepareVolume() OVERRIDE {
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
 public:
  virtual void SetUp() OVERRIDE {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    ASSERT_TRUE(volume_.SetUp());
    ExtensionApiTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    // The system service should be deleted by the time this function is
    // called hence no need to call RemoveObserver().
  }

 protected:
  virtual void PrepareVolume() OVERRIDE {
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

  virtual void SetUp() OVERRIDE {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    ASSERT_TRUE(drive_volume_.SetUp());
    ExtensionApiTest::SetUp();
  }

 protected:
  virtual void PrepareVolume() OVERRIDE {
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
  PrepareVolume();
  DoTestFileDisplay(&volume_);
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestKeyboardCopy) {
  PrepareVolume();
  DoTestKeyboardCopy(&volume_);
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestKeyboardDelete) {
  PrepareVolume();
  DoTestKeyboardDelete(&volume_);
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestOpenRecent) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("openSidebarRecent");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(hirono): Bring back the offline feature. http://crbug.com/238545
IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, DISABLED_TestOpenOffline) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("openSidebarOffline");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestOpenSharedWithMe) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("openSidebarSharedWithMe");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestAutocomplete) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("autocomplete");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromDriveToDownloads) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("transferFromDriveToDownloads");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromDownloadsToDrive) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("transferFromDownloadsToDrive");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromSharedToDownloads) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("transferFromSharedToDownloads");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromSharedToDrive) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("transferFromSharedToDrive");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromRecentToDownloads) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("transferFromRecentToDownloads");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       TransferFromRecentToDrive) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("transferFromRecentToDrive");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(hirono): Bring back the offline feature. http://crbug.com/238545
IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       DISABLED_TransferFromOfflineToDownloads) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("transferFromOfflineToDownloads");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(hirono): Bring back the offline feature. http://crbug.com/238545
IN_PROC_BROWSER_TEST_P(FileManagerBrowserTransferTest,
                       DISABLED_TransferFromOfflineToDrive) {
  PrepareVolume();
  ResultCatcher catcher;
  StartTest("transferFromOfflineToDrive");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}
}  // namespace
