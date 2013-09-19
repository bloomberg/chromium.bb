// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/sync_file_system/local/canned_syncable_file_system.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_status.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/local/syncable_file_operation_runner.h"
#include "chrome/browser/sync_file_system/local/syncable_file_system_operation.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/blob/mock_blob_url_request_context.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"

using fileapi::FileSystemOperation;
using fileapi::FileSystemURL;
using webkit_blob::MockBlobURLRequestContext;
using webkit_blob::ScopedTextBlob;
using base::PlatformFileError;

namespace sync_file_system {

namespace {
const std::string kParent = "foo";
const std::string kFile   = "foo/file";
const std::string kDir    = "foo/dir";
const std::string kChild  = "foo/dir/bar";
const std::string kOther  = "bar";
}  // namespace

class SyncableFileOperationRunnerTest : public testing::Test {
 protected:
  typedef FileSystemOperation::StatusCallback StatusCallback;

  // Use the current thread as IO thread so that we can directly call
  // operations in the tests.
  SyncableFileOperationRunnerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        file_system_(GURL("http://example.com"),
                     base::MessageLoopProxy::current().get(),
                     base::MessageLoopProxy::current().get()),
        callback_count_(0),
        write_status_(base::PLATFORM_FILE_ERROR_FAILED),
        write_bytes_(0),
        write_complete_(false),
        url_request_context_(file_system_.file_system_context()),
        weak_factory_(this) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    file_system_.SetUp();
    sync_context_ = new LocalFileSyncContext(
        dir_.path(),
        base::MessageLoopProxy::current().get(),
        base::MessageLoopProxy::current().get());
    ASSERT_EQ(
        SYNC_STATUS_OK,
        file_system_.MaybeInitializeFileSystemContext(sync_context_.get()));

    ASSERT_EQ(base::PLATFORM_FILE_OK, file_system_.OpenFileSystem());
    ASSERT_EQ(base::PLATFORM_FILE_OK,
              file_system_.CreateDirectory(URL(kParent)));
  }

  virtual void TearDown() OVERRIDE {
    if (sync_context_.get())
      sync_context_->ShutdownOnUIThread();
    sync_context_ = NULL;

    file_system_.TearDown();
    RevokeSyncableFileSystem();
  }

  FileSystemURL URL(const std::string& path) {
    return file_system_.URL(path);
  }

  LocalFileSyncStatus* sync_status() {
    return file_system_.backend()->sync_context()->sync_status();
  }

  void ResetCallbackStatus() {
    write_status_ = base::PLATFORM_FILE_ERROR_FAILED;
    write_bytes_ = 0;
    write_complete_ = false;
    callback_count_ = 0;
  }

  StatusCallback ExpectStatus(const tracked_objects::Location& location,
                              PlatformFileError expect) {
    return base::Bind(&SyncableFileOperationRunnerTest::DidFinish,
                      weak_factory_.GetWeakPtr(), location, expect);
  }

  FileSystemOperation::WriteCallback GetWriteCallback(
      const tracked_objects::Location& location) {
    return base::Bind(&SyncableFileOperationRunnerTest::DidWrite,
                      weak_factory_.GetWeakPtr(), location);
  }

  void DidWrite(const tracked_objects::Location& location,
                PlatformFileError status, int64 bytes, bool complete) {
    SCOPED_TRACE(testing::Message() << location.ToString());
    write_status_ = status;
    write_bytes_ += bytes;
    write_complete_ = complete;
    ++callback_count_;
  }

  void DidFinish(const tracked_objects::Location& location,
                 PlatformFileError expect, PlatformFileError status) {
    SCOPED_TRACE(testing::Message() << location.ToString());
    EXPECT_EQ(expect, status);
    ++callback_count_;
  }

  bool CreateTempFile(base::FilePath* path) {
    return file_util::CreateTemporaryFileInDir(dir_.path(), path);
  }

  ScopedEnableSyncFSDirectoryOperation enable_directory_operation_;
  base::ScopedTempDir dir_;

  content::TestBrowserThreadBundle thread_bundle_;
  CannedSyncableFileSystem file_system_;
  scoped_refptr<LocalFileSyncContext> sync_context_;

  int callback_count_;
  PlatformFileError write_status_;
  size_t write_bytes_;
  bool write_complete_;

  MockBlobURLRequestContext url_request_context_;

 private:
  base::WeakPtrFactory<SyncableFileOperationRunnerTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncableFileOperationRunnerTest);
};

TEST_F(SyncableFileOperationRunnerTest, SimpleQueue) {
  sync_status()->StartSyncing(URL(kFile));
  ASSERT_FALSE(sync_status()->IsWritable(URL(kFile)));

  // The URL is in syncing so the write operations won't run.
  ResetCallbackStatus();
  file_system_.operation_runner()->CreateFile(
      URL(kFile), false /* exclusive */,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  file_system_.operation_runner()->Truncate(
      URL(kFile), 1,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, callback_count_);

  // Read operations are not blocked (and are executed before queued ones).
  file_system_.operation_runner()->FileExists(
      URL(kFile), ExpectStatus(FROM_HERE, base::PLATFORM_FILE_ERROR_NOT_FOUND));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // End syncing (to enable write).
  sync_status()->EndSyncing(URL(kFile));
  ASSERT_TRUE(sync_status()->IsWritable(URL(kFile)));

  ResetCallbackStatus();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, callback_count_);

  // Now the file must have been created and updated.
  ResetCallbackStatus();
  file_system_.operation_runner()->FileExists(
      URL(kFile), ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
}

TEST_F(SyncableFileOperationRunnerTest, WriteToParentAndChild) {
  // First create the kDir directory and kChild in the dir.
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_.CreateDirectory(URL(kDir)));
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_.CreateFile(URL(kChild)));

  // Start syncing the kDir directory.
  sync_status()->StartSyncing(URL(kDir));
  ASSERT_FALSE(sync_status()->IsWritable(URL(kDir)));

  // Writes to kParent and kChild should be all queued up.
  ResetCallbackStatus();
  file_system_.operation_runner()->Truncate(
      URL(kChild), 1, ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  file_system_.operation_runner()->Remove(
      URL(kParent), true /* recursive */,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, callback_count_);

  // Read operations are not blocked (and are executed before queued ones).
  file_system_.operation_runner()->DirectoryExists(
      URL(kDir), ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Writes to unrelated files must succeed as well.
  ResetCallbackStatus();
  file_system_.operation_runner()->CreateDirectory(
      URL(kOther), false /* exclusive */, false /* recursive */,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // End syncing (to enable write).
  sync_status()->EndSyncing(URL(kDir));
  ASSERT_TRUE(sync_status()->IsWritable(URL(kDir)));

  ResetCallbackStatus();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, callback_count_);
}

TEST_F(SyncableFileOperationRunnerTest, CopyAndMove) {
  // First create the kDir directory and kChild in the dir.
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_.CreateDirectory(URL(kDir)));
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_.CreateFile(URL(kChild)));

  // Start syncing the kParent directory.
  sync_status()->StartSyncing(URL(kParent));

  // Copying kDir to other directory should succeed, while moving would fail
  // (since the source directory is in syncing).
  ResetCallbackStatus();
  file_system_.operation_runner()->Copy(
      URL(kDir), URL("dest-copy"),
      fileapi::FileSystemOperationRunner::CopyProgressCallback(),
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  file_system_.operation_runner()->Move(
      URL(kDir), URL("dest-move"),
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Only "dest-copy1" should exist.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.DirectoryExists(URL("dest-copy")));
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            file_system_.DirectoryExists(URL("dest-move")));

  // Start syncing the "dest-copy2" directory.
  sync_status()->StartSyncing(URL("dest-copy2"));

  // Now the destination is also locked copying kDir should be queued.
  ResetCallbackStatus();
  file_system_.operation_runner()->Copy(
      URL(kDir), URL("dest-copy2"),
      fileapi::FileSystemOperationRunner::CopyProgressCallback(),
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, callback_count_);

  // Finish syncing the "dest-copy2" directory to unlock Copy.
  sync_status()->EndSyncing(URL("dest-copy2"));
  ResetCallbackStatus();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Now we should have "dest-copy2".
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.DirectoryExists(URL("dest-copy2")));

  // Finish syncing the kParent to unlock Move.
  sync_status()->EndSyncing(URL(kParent));
  ResetCallbackStatus();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Now we should have "dest-move".
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_.DirectoryExists(URL("dest-move")));
}

TEST_F(SyncableFileOperationRunnerTest, Write) {
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_.CreateFile(URL(kFile)));
  const std::string kData("Lorem ipsum.");
  ScopedTextBlob blob(url_request_context_, "blob:foo", kData);

  sync_status()->StartSyncing(URL(kFile));

  ResetCallbackStatus();
  file_system_.operation_runner()->Write(
      &url_request_context_,
      URL(kFile), blob.GetBlobDataHandle(), 0, GetWriteCallback(FROM_HERE));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, callback_count_);

  sync_status()->EndSyncing(URL(kFile));
  ResetCallbackStatus();

  while (!write_complete_)
    base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(base::PLATFORM_FILE_OK, write_status_);
  EXPECT_EQ(kData.size(), write_bytes_);
  EXPECT_TRUE(write_complete_);
}

TEST_F(SyncableFileOperationRunnerTest, QueueAndCancel) {
  sync_status()->StartSyncing(URL(kFile));
  ASSERT_FALSE(sync_status()->IsWritable(URL(kFile)));

  ResetCallbackStatus();
  file_system_.operation_runner()->CreateFile(
      URL(kFile), false /* exclusive */,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_ERROR_ABORT));
  file_system_.operation_runner()->Truncate(
      URL(kFile), 1,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_ERROR_ABORT));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, callback_count_);

  ResetCallbackStatus();

  // This shouldn't crash nor leak memory.
  sync_context_->ShutdownOnUIThread();
  sync_context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, callback_count_);
}

// Test if CopyInForeignFile runs cooperatively with other Sync operations.
TEST_F(SyncableFileOperationRunnerTest, CopyInForeignFile) {
  const std::string kTestData("test data");

  base::FilePath temp_path;
  ASSERT_TRUE(CreateTempFile(&temp_path));
  ASSERT_EQ(static_cast<int>(kTestData.size()),
            file_util::WriteFile(
                temp_path, kTestData.data(), kTestData.size()));

  sync_status()->StartSyncing(URL(kFile));
  ASSERT_FALSE(sync_status()->IsWritable(URL(kFile)));

  // The URL is in syncing so CopyIn (which is a write operation) won't run.
  ResetCallbackStatus();
  file_system_.operation_runner()->CopyInForeignFile(
      temp_path, URL(kFile),
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(0, callback_count_);

  // End syncing (to enable write).
  sync_status()->EndSyncing(URL(kFile));
  ASSERT_TRUE(sync_status()->IsWritable(URL(kFile)));

  ResetCallbackStatus();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Now the file must have been created and have the same content as temp_path.
  ResetCallbackStatus();
  file_system_.DoVerifyFile(
      URL(kFile), kTestData,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
}

TEST_F(SyncableFileOperationRunnerTest, Cancel) {
  // Prepare a file.
  file_system_.operation_runner()->CreateFile(
      URL(kFile), false /* exclusive */,
      ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Run Truncate and immediately cancel. This shouldn't crash.
  ResetCallbackStatus();
  fileapi::FileSystemOperationRunner::OperationID id =
      file_system_.operation_runner()->Truncate(
          URL(kFile), 10,
          ExpectStatus(FROM_HERE, base::PLATFORM_FILE_OK));
  file_system_.operation_runner()->Cancel(
      id, ExpectStatus(FROM_HERE, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(2, callback_count_);
}

}  // namespace sync_file_system
