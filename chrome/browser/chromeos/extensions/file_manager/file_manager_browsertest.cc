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
#include "chrome/browser/chromeos/drive/drive_file_system.h"
#include "chrome/browser/chromeos/drive/drive_file_system_observer.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
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

const char kKeyboardTestFileName[] = "world.mpeg";
const int64 kKeyboardTestFileSize = 1000;
const char kKeyboardTestFileCopyName[] = "world (1).mpeg";

// Test suffixes appended to the Javascript tests' names.
const char kDownloadsVolume[] = "Downloads";
const char kDriveVolume[] = "Drive";

struct TestFileInfo {
  const char* base_name;
  int64 file_size;
  const char* last_modified_time_as_string;
} kTestFiles[] = {
  { "hello.txt", 123, "4 Sep 1998 12:34:56" },
  { "My Desktop Background.png", 1024, "18 Jan 2038 01:02:03" },
  { kKeyboardTestFileName, kKeyboardTestFileSize, "4 July 2012 10:35:00" },
};

struct TestDirectoryInfo {
  const char* base_name;
  const char* last_modified_time_as_string;
} kTestDirectories[] = {
  { "photos", "1 Jan 1980 23:59:59" },
  // Files starting with . are filtered out in
  // file_manager/js/directory_contents.js, so this should not be shown.
  { ".warez", "26 Oct 1985 13:39" },
};

// The base test class. Used by FileManagerBrowserLocalTest and
// FileManagerBrowserDriveTest.
// TODO(satorux): Add the latter: crbug.com/224534.
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
  void CreateTestFilesAndDirectories();

  // Creates a file with the given |name|, |length|, and |modification_time|.
  virtual void CreateTestFile(const std::string& name,
                              int64 length,
                              const std::string& modification_time) = 0;

  // Creates an empty directory with the given |name| and |modification_time|.
  virtual void CreateTestDirectory(
      const std::string& name,
      const std::string& modification_time) = 0;

  // Returns the path of the root directory.
  virtual base::FilePath GetRootPath() = 0;

  // Returns true if |file_path| exists.
  virtual bool PathExists(const base::FilePath& file_path) = 0;

  // Waits until a file with the given size is present at |path|. Returns
  // true on success.
  virtual bool WaitUntilFilePresentWithSize(const base::FilePath& file_path,
                                            int64 file_size) = 0;

  // Waits until a file is not present at |path|. Returns true on success.
  virtual bool WaitUntilFileNotPresent(const base::FilePath& file_path)
      = 0;

  // Runs the file display test on the passed |volume|, shared by subclasses.
  void DoTestFileDisplay(const std::string& volume);

  // Runs the keyboard copy test on the passed |volume|, shared by subclasses.
  void DoTestKeyboardCopy(const std::string& volume);

  // Runs the keyboard delete test on the passed |volume|, shared by subclasses.
  void DoTestKeyboardDelete(const std::string& volume);
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

void FileManagerBrowserTestBase::CreateTestFilesAndDirectories() {
  for (size_t i = 0; i < arraysize(kTestFiles); ++i) {
    CreateTestFile(kTestFiles[i].base_name,
                   kTestFiles[i].file_size,
                   kTestFiles[i].last_modified_time_as_string);
  }
  for (size_t i = 0; i < arraysize(kTestDirectories); ++i) {
    CreateTestDirectory(kTestDirectories[i].base_name,
                        kTestDirectories[i].last_modified_time_as_string);
  }
}

void FileManagerBrowserTestBase::DoTestFileDisplay(const std::string& volume) {
  ResultCatcher catcher;
  StartTest("fileDisplay" + volume);

  ExtensionTestMessageListener listener("initial check done", true);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  CreateTestFile("newly added file.mp3", 2000, "4 Sep 1998 00:00:00");
  listener.Reply("file added");

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

void FileManagerBrowserTestBase::DoTestKeyboardCopy(const std::string& volume) {
  base::FilePath copy_path =
      GetRootPath().AppendASCII(kKeyboardTestFileCopyName);
  ASSERT_FALSE(PathExists(copy_path));

  ResultCatcher catcher;
  StartTest("keyboardCopy" + volume);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(WaitUntilFilePresentWithSize(copy_path, kKeyboardTestFileSize));

  // Check that it was a copy, not a move.
  base::FilePath source_path =
      GetRootPath().AppendASCII(kKeyboardTestFileName);
  ASSERT_TRUE(PathExists(source_path));
}

void FileManagerBrowserTestBase::DoTestKeyboardDelete(
    const std::string& volume) {
  base::FilePath delete_path =
      GetRootPath().AppendASCII(kKeyboardTestFileName);
  ASSERT_TRUE(PathExists(delete_path));

  ResultCatcher catcher;
  StartTest("keyboardDelete" + volume);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(WaitUntilFileNotPresent(delete_path));
}

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
  DCHECK(ok);

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

class FileManagerBrowserLocalTest : public FileManagerBrowserTestBase {
 public:
  virtual void SetUp() OVERRIDE {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    downloads_path_ = tmp_dir_.path().Append("Downloads");
    ASSERT_TRUE(file_util::CreateDirectory(downloads_path_));

    CreateTestFilesAndDirectories();

    ExtensionApiTest::SetUp();
  }

 protected:
  // FileManagerBrowserTestBase overrides.
  virtual void CreateTestFile(const std::string& name,
                              int64 length,
                              const std::string& modification_time) OVERRIDE;
  virtual void CreateTestDirectory(
      const std::string& name,
      const std::string& modification_time) OVERRIDE;
  virtual base::FilePath GetRootPath() OVERRIDE;
  virtual bool PathExists(const base::FilePath& file_path) OVERRIDE;
  virtual bool WaitUntilFilePresentWithSize(const base::FilePath& file_path,
                                            int64 file_size) OVERRIDE;
  virtual bool WaitUntilFileNotPresent(const base::FilePath& file_path)
      OVERRIDE;

  // Add a mount point to the fake Downloads directory. Should be called
  // before StartTest().
  void AddMountPointToFakeDownloads();

  // Path to the fake Downloads directory used in the test.
  base::FilePath downloads_path_;

 private:
  base::ScopedTempDir tmp_dir_;
};

// InGuestMode tests temporarily disabled due to the crbug.com/230724 bug.
//
// INSTANTIATE_TEST_CASE_P(InGuestMode,
//                         FileManagerBrowserLocalTest,
//                         ::testing::Values(true));

INSTANTIATE_TEST_CASE_P(InNonGuestMode,
                        FileManagerBrowserLocalTest,
                        ::testing::Values(false));

void FileManagerBrowserLocalTest::CreateTestFile(
    const std::string& name,
    int64 length,
    const std::string& modification_time) {
  ASSERT_GE(length, 0);
  base::FilePath path = downloads_path_.AppendASCII(name);
  int flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE;
  bool created = false;
  base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFile file = base::CreatePlatformFile(path, flags,
                                                     &created, &error);
  ASSERT_TRUE(created);
  ASSERT_FALSE(error) << error;
  ASSERT_TRUE(base::TruncatePlatformFile(file, length));
  ASSERT_TRUE(base::ClosePlatformFile(file));
  base::Time time;
  ASSERT_TRUE(base::Time::FromString(modification_time.c_str(), &time));
  ASSERT_TRUE(file_util::SetLastModifiedTime(path, time));
}

void FileManagerBrowserLocalTest::CreateTestDirectory(
    const std::string& name,
    const std::string& modification_time) {
  base::FilePath path = downloads_path_.AppendASCII(name);
  ASSERT_TRUE(file_util::CreateDirectory(path));
  base::Time time;
  ASSERT_TRUE(base::Time::FromString(modification_time.c_str(), &time));
  ASSERT_TRUE(file_util::SetLastModifiedTime(path, time));
}

base::FilePath FileManagerBrowserLocalTest::GetRootPath() {
  return downloads_path_;
}

bool FileManagerBrowserLocalTest::PathExists(const base::FilePath& file_path) {
  return file_util::PathExists(file_path);
}

bool FileManagerBrowserLocalTest::WaitUntilFilePresentWithSize(
    const base::FilePath& file_path,
    int64 file_size) {
  TestFilePathWatcher watcher(file_path, base::Bind(FilePresentWithSize,
                                                    file_size));
  return watcher.RunMessageLoopUntilConditionSatisfied();
}

bool FileManagerBrowserLocalTest::WaitUntilFileNotPresent(
    const base::FilePath& file_path) {
  TestFilePathWatcher watcher(file_path, base::Bind(FileNotPresent));
  return watcher.RunMessageLoopUntilConditionSatisfied();
}

void FileManagerBrowserLocalTest::AddMountPointToFakeDownloads() {
  // Install our fake Downloads mount point first.
  fileapi::ExternalMountPoints* mount_points =
      content::BrowserContext::GetMountPoints(profile());
  ASSERT_TRUE(mount_points->RevokeFileSystem("Downloads"));
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "Downloads", fileapi::kFileSystemTypeNativeLocal, downloads_path_));
}

class FileManagerBrowserDriveTest : public FileManagerBrowserTestBase,
                                    public drive::DriveFileSystemObserver {
 public:
  FileManagerBrowserDriveTest()
      : fake_drive_service_(NULL),
        system_service_(NULL),
        waiting_for_directory_change_(false) {
  }

  virtual void SetUp() OVERRIDE {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

    ASSERT_TRUE(test_cache_root_.CreateUniqueTempDir());

    drive::DriveSystemServiceFactory::SetFactoryForTest(
        base::Bind(&FileManagerBrowserDriveTest::CreateDriveSystemService,
                   base::Unretained(this)));

    ExtensionApiTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    // The system service should be deleted by the time this function is
    // called hence no need to call RemoveObserver().
  }

 protected:
  // FileManagerBrowserTestBase overrides.
  virtual void CreateTestFile(const std::string& name,
                              int64 length,
                              const std::string& modification_time) OVERRIDE;
  virtual void CreateTestDirectory(
      const std::string& name,
      const std::string& modification_time) OVERRIDE;
  virtual base::FilePath GetRootPath() OVERRIDE;
  virtual bool PathExists(const base::FilePath& file_path) OVERRIDE;
  virtual bool WaitUntilFilePresentWithSize(const base::FilePath& file_path,
                                            int64 file_size) OVERRIDE;
  virtual bool WaitUntilFileNotPresent(const base::FilePath& file_path)
      OVERRIDE;

  // Waits until a notification for a directory change is received.
  void WaitUntilDirectoryChanged();

  // Returns true if a file is not present at |file_path|.
  bool FileIsNotPresent(const base::FilePath& file_path);

  // Returns true if a file of the size |file_size| is present at |file_path|.
  bool FileIsPresentWithSize(const base::FilePath& file_path,
                             int64 file_size);

  // Notifies DriveFileSystem that the contents in FakeDriveService are
  // changed, hence the new contents should be fetched.
  void CheckForUpdates();

  // DriveSystemService factory function for this test.
  drive::DriveSystemService* CreateDriveSystemService(Profile* profile);

  // DriveFileSystemObserver override.
  //
  // The event is used to unblock WaitUntilDirectoryChanged().
  virtual void OnDirectoryChanged(
      const base::FilePath& directory_path) OVERRIDE;

  base::ScopedTempDir test_cache_root_;
  google_apis::FakeDriveService* fake_drive_service_;
  drive::DriveSystemService* system_service_;
  bool waiting_for_directory_change_;
};

// Don't test Drive in the guest mode as it's not supported.
INSTANTIATE_TEST_CASE_P(InNonGuestMode,
                        FileManagerBrowserDriveTest,
                        ::testing::Values(false));

void FileManagerBrowserDriveTest::CreateTestFile(
    const std::string& name,
    int64 length,
    const std::string& modification_time) {
  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> resource_entry;
  fake_drive_service_->AddNewFile(
      "text/plain",
      length,
      fake_drive_service_->GetRootResourceId(),
      name,
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

void FileManagerBrowserDriveTest::CreateTestDirectory(
    const std::string& name,
    const std::string& modification_time) {
  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> resource_entry;
  fake_drive_service_->AddNewDirectory(
      fake_drive_service_->GetRootResourceId(),
      name,
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

base::FilePath FileManagerBrowserDriveTest::GetRootPath() {
  return base::FilePath(drive::util::kDriveMyDriveRootPath);
}

bool FileManagerBrowserDriveTest::PathExists(const base::FilePath& file_path) {
  DCHECK(system_service_);
  DCHECK(system_service_->file_system());

  drive::DriveFileError error = drive::DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<drive::DriveEntryProto> entry_proto;
  system_service_->file_system()->GetEntryInfoByPath(
      file_path,
      google_apis::test_util::CreateCopyResultCallback(&error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();

  return error == drive::DRIVE_FILE_OK;
}

bool FileManagerBrowserDriveTest::WaitUntilFilePresentWithSize(
    const base::FilePath& file_path,
    int64 file_size) {
  while (true) {
    if (FileIsPresentWithSize(file_path, file_size))
      return true;
    WaitUntilDirectoryChanged();
  }
  NOTREACHED();
  return false;
}

bool FileManagerBrowserDriveTest::WaitUntilFileNotPresent(
    const base::FilePath& file_path) {
  while (true) {
    if (FileIsNotPresent(file_path))
      return true;
    WaitUntilDirectoryChanged();
  }
  NOTREACHED();
  return false;
}

void FileManagerBrowserDriveTest::WaitUntilDirectoryChanged() {
  waiting_for_directory_change_ = true;
  MessageLoop::current()->Run();
  waiting_for_directory_change_ = false;
}

bool FileManagerBrowserDriveTest::FileIsPresentWithSize(
    const base::FilePath& file_path,
    int64 file_size) {
  DCHECK(system_service_);
  DCHECK(system_service_->file_system());

  drive::DriveFileError error = drive::DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<drive::DriveEntryProto> entry_proto;
  system_service_->file_system()->GetEntryInfoByPath(
      file_path,
      google_apis::test_util::CreateCopyResultCallback(&error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();

  return (error == drive::DRIVE_FILE_OK &&
          entry_proto->file_info().size() == file_size);
}

bool FileManagerBrowserDriveTest::FileIsNotPresent(
    const base::FilePath& file_path) {
  DCHECK(system_service_);
  DCHECK(system_service_->file_system());

  drive::DriveFileError error = drive::DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<drive::DriveEntryProto> entry_proto;
  system_service_->file_system()->GetEntryInfoByPath(
      file_path,
      google_apis::test_util::CreateCopyResultCallback(&error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();

  return error == drive::DRIVE_FILE_ERROR_NOT_FOUND;
}

void FileManagerBrowserDriveTest::CheckForUpdates() {
  if (system_service_ && system_service_->file_system()) {
    system_service_->file_system()->CheckForUpdates();
  }
}

drive::DriveSystemService*
FileManagerBrowserDriveTest::CreateDriveSystemService(Profile* profile) {
  fake_drive_service_ = new google_apis::FakeDriveService;
  fake_drive_service_->LoadResourceListForWapi(
      "chromeos/gdata/empty_feed.json");
  fake_drive_service_->LoadAccountMetadataForWapi(
      "chromeos/gdata/account_metadata.json");
  fake_drive_service_->LoadAppListForDriveApi("chromeos/drive/applist.json");

  // Create test files and directories inside the fake drive service.
  CreateTestFilesAndDirectories();

  system_service_ = new drive::DriveSystemService(profile,
                                                  fake_drive_service_,
                                                  test_cache_root_.path(),
                                                  NULL);
  system_service_->file_system()->AddObserver(this);

  return system_service_;
}

void FileManagerBrowserDriveTest::OnDirectoryChanged(
    const base::FilePath& directory_path) {
  if (waiting_for_directory_change_)
    MessageLoop::current()->Quit();
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserLocalTest, TestFileDisplay) {
  AddMountPointToFakeDownloads();
  DoTestFileDisplay(kDownloadsVolume);
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserLocalTest, TestKeyboardCopy) {
  AddMountPointToFakeDownloads();
  DoTestKeyboardCopy(kDownloadsVolume);
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserLocalTest, TestKeyboardDelete) {
  AddMountPointToFakeDownloads();
  DoTestKeyboardDelete(kDownloadsVolume);
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestFileDisplay) {
  drive_test_util::WaitUntilDriveMountPointIsAdded(browser()->profile());
  DoTestFileDisplay(kDriveVolume);
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestKeyboardCopy) {
  drive_test_util::WaitUntilDriveMountPointIsAdded(browser()->profile());
  DoTestKeyboardCopy(kDriveVolume);
}

IN_PROC_BROWSER_TEST_P(FileManagerBrowserDriveTest, TestKeyboardDelete) {
  drive_test_util::WaitUntilDriveMountPointIsAdded(browser()->profile());
  DoTestKeyboardDelete(kDriveVolume);
}

}  // namespace
