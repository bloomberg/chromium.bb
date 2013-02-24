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
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "webkit/fileapi/external_mount_points.h"

namespace {

const char kFileManagerExtensionId[] = "hhaomjibdihmijegdhdafkllkbggdgoj";

const char kKeyboardTestFileName[] = "world.mpeg";
const int kKeyboardTestFileSize = 1000;
const char kKeyboardTestFileCopyName[] = "world (1).mpeg";

class FileManagerBrowserTest : public ExtensionApiTest {
 public:
  virtual void SetUp() OVERRIDE {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    downloads_path_ = tmp_dir_.path().Append("Downloads");
    ASSERT_TRUE(file_util::CreateDirectory(downloads_path_));

    CreateTestFile("hello.txt", 123, "4 Sep 1998 12:34:56");
    CreateTestFile("My Desktop Background.png", 1024, "18 Jan 2038 01:02:03");
    CreateTestFile(kKeyboardTestFileName, kKeyboardTestFileSize,
                   "4 July 2012 10:35:00");
    CreateTestDirectory("photos", "1 Jan 1980 23:59:59");
    // Files starting with . are filtered out in
    // file_manager/js/directory_contents.js, so this should not be shown.
    CreateTestDirectory(".warez", "26 Oct 1985 13:39");

    ExtensionApiTest::SetUp();
  }

 protected:
  // Creates a file with the given |name|, |length|, and |modification_time|.
  void CreateTestFile(const std::string& name,
                      int length,
                      const std::string& modification_time);

  // Creates an empty directory with the given |name| and |modification_time|.
  void CreateTestDirectory(const std::string& name,
                           const std::string& modification_time);

  // Loads the file manager extension, giving it a fake Downloads folder for
  // testing, and waits for it to finish initializing. This is invoked at the
  // start of each test (it crashes if run in SetUp).
  void StartFileManager();

  // By default the file manager waits 30 seconds before deleting a file to give
  // the user a chance to undo. This hack removes the delay for testing.
  void SetShorterDeleteTimeout();

  // Reduces the directory refresh interval from 1000ms to 100ms.
  void SetShorterRefreshInterval();

  // Loads our testing extension and sends it a string identifying the current
  // test.
  void StartTest(const std::string& test_name);

  base::FilePath downloads_path_;

 private:
  base::ScopedTempDir tmp_dir_;
};

void FileManagerBrowserTest::CreateTestFile(
    const std::string& name,
    int length,
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

void FileManagerBrowserTest::CreateTestDirectory(
    const std::string& name,
    const std::string& modification_time) {
  base::FilePath path = downloads_path_.AppendASCII(name);
  ASSERT_TRUE(file_util::CreateDirectory(path));
  base::Time time;
  ASSERT_TRUE(base::Time::FromString(modification_time.c_str(), &time));
  ASSERT_TRUE(file_util::SetLastModifiedTime(path, time));
}

void FileManagerBrowserTest::StartFileManager() {
  // Install our fake Downloads mount point first.
  fileapi::ExternalMountPoints* mount_points =
      content::BrowserContext::GetMountPoints(profile());
  ASSERT_TRUE(mount_points->RevokeFileSystem("Downloads"));
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "Downloads", fileapi::kFileSystemTypeNativeLocal, downloads_path_));

  std::string kFileManagerUrl = std::string("chrome-extension://") +
      kFileManagerExtensionId + "/main.html#Downloads";
  ui_test_utils::NavigateToURL(browser(), GURL(kFileManagerUrl));

  // This is sent by the file manager when it's finished initializing.
  ExtensionTestMessageListener listener("worker-initialized", false);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
}

void FileManagerBrowserTest::SetShorterDeleteTimeout() {
  const char set_timeout_code[] = "FileCopyManager.DELETE_TIMEOUT = 0";
  // FileCopyManager runs in the background page.
  content::RenderViewHost* host =
      extensions::ExtensionSystem::Get(profile())->process_manager()->
      GetBackgroundHostForExtension(kFileManagerExtensionId)->
      render_view_host();
  host->ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16(set_timeout_code));
}

void FileManagerBrowserTest::SetShorterRefreshInterval() {
  const char set_timeout_code[] = "SIMULTANEOUS_RESCAN_INTERVAL = 100";
  content::RenderViewHost* host =
      browser()->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost();
  host->ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16(set_timeout_code));
}

void FileManagerBrowserTest::StartTest(const std::string& test_name) {
  base::FilePath path = test_data_dir_.AppendASCII("file_manager_browsertest");
  const extensions::Extension* extension = LoadExtensionAsComponent(path);
  ASSERT_TRUE(extension);

  ExtensionTestMessageListener listener("which test", true);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(test_name);
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

  // Starts the FilePathWatcher and returns once it's watching for changes.
  void StartAndWaitUntilReady();

  // Waits (running a message pump) until the callback returns true or
  // FilePathWatcher reports an error. Return true on success.
  bool RunMessageLoopUntilConditionSatisfied();

 private:
  // FILE thread callback to start the FilePathWatcher.
  void StartWatching();

  // FilePathWatcher callback (on the FILE thread). Posts Done() to the UI
  // thread when the condition is satisfied or there is an error.
  void FilePathWatcherCallback(const base::FilePath& path, bool error);

  // Sets done_ and stops the message pump if running.
  void Done();

  const base::FilePath path_;
  ConditionCallback condition_;
  scoped_ptr<base::FilePathWatcher> watcher_;
  base::Closure quit_closure_;
  bool done_;
  bool error_;
};

TestFilePathWatcher::TestFilePathWatcher(const base::FilePath& path,
                                         const ConditionCallback& condition)
    : path_(path),
      condition_(condition),
      done_(false),
      error_(false) {
}

void TestFilePathWatcher::StartAndWaitUntilReady() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::RunLoop run_loop;
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&TestFilePathWatcher::StartWatching,
                 base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();
}

void TestFilePathWatcher::StartWatching() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  watcher_.reset(new base::FilePathWatcher);
  bool ok = watcher_->Watch(
      path_, false /*recursive*/,
      base::Bind(&TestFilePathWatcher::FilePathWatcherCallback,
                 base::Unretained(this)));
  ASSERT_TRUE(ok);
}

void TestFilePathWatcher::FilePathWatcherCallback(const base::FilePath& path,
                                                  bool error) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  ASSERT_EQ(path_, path);
  if (error || condition_.Run(path)) {
    error_ = error;
    watcher_.reset();
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&TestFilePathWatcher::Done, base::Unretained(this)));
  }
}

void TestFilePathWatcher::Done() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  done_ = true;
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

bool TestFilePathWatcher::RunMessageLoopUntilConditionSatisfied() {
  if (done_)
    return !error_;
  base::RunLoop message_loop_runner;
  quit_closure_ = message_loop_runner.QuitClosure();
  message_loop_runner.Run();
  quit_closure_ = base::Closure();
  return !error_;
}

bool CopiedFilePresent(const base::FilePath& path) {
  int64 copy_size = 0;
  // If the file doesn't exist yet this will fail and we'll keep waiting.
  if (!file_util::GetFileSize(path, &copy_size))
    return false;
  return (copy_size == kKeyboardTestFileSize);
}

bool DeletedFileGone(const base::FilePath& path) {
  return !file_util::PathExists(path);
};

IN_PROC_BROWSER_TEST_F(FileManagerBrowserTest, TestFileDisplay) {
  StartFileManager();
  SetShorterRefreshInterval();

  ResultCatcher catcher;

  StartTest("file display");

  ExtensionTestMessageListener listener("initial check done", true);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  CreateTestFile("newly added file.mp3", 2000, "4 Sep 1998 00:00:00");
  listener.Reply("file added");

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(FileManagerBrowserTest, TestKeyboardCopy) {
  StartFileManager();
  SetShorterRefreshInterval();

  base::FilePath copy_path =
      downloads_path_.AppendASCII(kKeyboardTestFileCopyName);
  ASSERT_FALSE(file_util::PathExists(copy_path));
  TestFilePathWatcher watcher(copy_path, base::Bind(CopiedFilePresent));
  watcher.StartAndWaitUntilReady();

  ResultCatcher catcher;
  StartTest("keyboard copy");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ASSERT_TRUE(watcher.RunMessageLoopUntilConditionSatisfied());

  // Check that it was a copy, not a move.
  base::FilePath source_path =
      downloads_path_.AppendASCII(kKeyboardTestFileName);
  ASSERT_TRUE(file_util::PathExists(source_path));
}

IN_PROC_BROWSER_TEST_F(FileManagerBrowserTest, TestKeyboardDelete) {
  StartFileManager();
  SetShorterRefreshInterval();

  // There is currently no check that this call does what it's supposed to do.
  // If it doesn't, this test will take more than 30 seconds to complete.
  SetShorterDeleteTimeout();

  base::FilePath delete_path =
      downloads_path_.AppendASCII(kKeyboardTestFileName);
  ASSERT_TRUE(file_util::PathExists(delete_path));
  TestFilePathWatcher watcher(delete_path, base::Bind(DeletedFileGone));
  watcher.StartAndWaitUntilReady();

  ResultCatcher catcher;
  StartTest("keyboard delete");
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ASSERT_TRUE(watcher.RunMessageLoopUntilConditionSatisfied());
}

}  // namespace
