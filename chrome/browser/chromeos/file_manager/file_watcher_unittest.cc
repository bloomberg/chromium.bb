// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_watcher.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace {

using google_apis::test_util::CreateQuitCallback;
using google_apis::test_util::CreateCopyResultCallback;

class FileManagerFileWatcherTest : public testing::Test {
 public:
  // Use IO_MAINLOOP so FilePathWatcher works in the fake FILE thread, which
  // is actually shared with the main thread.
  FileManagerFileWatcherTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(FileManagerFileWatcherTest, AddAndRemoveOneExtensionId) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionId[] = "extension-id";

  FileWatcher file_watcher(kVirtualPath);
  file_watcher.AddExtension(kExtensionId);
  std::vector<std::string> extension_ids = file_watcher.GetExtensionIds();

  ASSERT_EQ(1U, extension_ids.size());
  ASSERT_EQ(kExtensionId, extension_ids[0]);

  file_watcher.RemoveExtension(kExtensionId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(0U, extension_ids.size());
}

TEST_F(FileManagerFileWatcherTest, AddAndRemoveMultipleExtensionIds) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionFooId[] = "extension-foo-id";
  const char kExtensionBarId[] = "extension-bar-id";

  FileWatcher file_watcher(kVirtualPath);
  file_watcher.AddExtension(kExtensionFooId);
  file_watcher.AddExtension(kExtensionBarId);
  std::vector<std::string> extension_ids = file_watcher.GetExtensionIds();

  // The list should be sorted.
  ASSERT_EQ(2U, extension_ids.size());
  ASSERT_EQ(kExtensionBarId, extension_ids[0]);
  ASSERT_EQ(kExtensionFooId, extension_ids[1]);

  // Remove Foo. Bar should remain.
  file_watcher.RemoveExtension(kExtensionFooId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(1U, extension_ids.size());
  ASSERT_EQ(kExtensionBarId, extension_ids[0]);

  // Remove Bar. Nothing should remain.
  file_watcher.RemoveExtension(kExtensionBarId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(0U, extension_ids.size());
}

TEST_F(FileManagerFileWatcherTest, AddSameExtensionMultipleTimes) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionId[] = "extension-id";

  FileWatcher file_watcher(kVirtualPath);
  // Add three times.
  file_watcher.AddExtension(kExtensionId);
  file_watcher.AddExtension(kExtensionId);
  file_watcher.AddExtension(kExtensionId);

  std::vector<std::string> extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(1U, extension_ids.size());
  ASSERT_EQ(kExtensionId, extension_ids[0]);

  // Remove 1st time.
  file_watcher.RemoveExtension(kExtensionId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(1U, extension_ids.size());

  // Remove 2nd time.
  file_watcher.RemoveExtension(kExtensionId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(1U, extension_ids.size());

  // Remove 3rd time. The extension ID should be gone now.
  file_watcher.RemoveExtension(kExtensionId);
  extension_ids = file_watcher.GetExtensionIds();
  ASSERT_EQ(0U, extension_ids.size());
}

TEST_F(FileManagerFileWatcherTest, WatchLocalFile) {
  const base::FilePath kVirtualPath =
      base::FilePath::FromUTF8Unsafe("foo/bar.txt");
  const char kExtensionId[] = "extension-id";

  // Create a temporary directory.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // See the comment at the end of this function for why scoped_ptr is used.
  std::unique_ptr<FileWatcher> file_watcher(new FileWatcher(kVirtualPath));
  file_watcher->AddExtension(kExtensionId);

  // Start watching changes in the temporary directory.
  base::FilePath changed_path;
  bool watcher_created = false;
  bool on_change_error = false;
  base::RunLoop run_loop;
  file_watcher->WatchLocalFile(
      temp_dir.GetPath(),
      CreateQuitCallback(
          &run_loop, CreateCopyResultCallback(&changed_path, &on_change_error)),
      CreateCopyResultCallback(&watcher_created));
  // Spin the message loop so the base::FilePathWatcher is created.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(watcher_created);

  // Create a temporary file in the temporary directory. The file watcher
  // should detect the change in the directory.
  base::FilePath temp_file_path;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir.GetPath(), &temp_file_path));
  // Wait until the directory change is notified.
  run_loop.Run();
  ASSERT_FALSE(on_change_error);
  ASSERT_EQ(temp_dir.GetPath().value(), changed_path.value());

  // This is ugly, but FileWatcher should be deleted explicitly here, and
  // spin the message loop so the base::FilePathWatcher is deleted.
  // Otherwise, base::FilePathWatcher may detect a change when the temporary
  // directory is deleted, which may result in crash.
  file_watcher.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace file_manager.
