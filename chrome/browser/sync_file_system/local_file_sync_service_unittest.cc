// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync_file_system/local_file_sync_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/syncable/canned_syncable_file_system.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using content::BrowserThread;
using content::TestBrowserThread;
using fileapi::FileChange;
using fileapi::FileChangeList;
using fileapi::FileSystemURL;
using fileapi::SyncFileType;
using fileapi::SyncStatusCode;

namespace sync_file_system {

namespace {

const char kOrigin[] = "http://example.com";
const char kServiceName[] = "test";

template <typename R>
void Assign(const base::Closure& closure, R* result_out, R result) {
  DCHECK(result_out);
  *result_out = result;
  closure.Run();
}

template <typename R> base::Callback<void(R)>
AssignAndQuitCallback(base::RunLoop* run_loop, R* result) {
  return base::Bind(&Assign<R>, run_loop->QuitClosure(),
                    base::Unretained(result));
}

void DidPrepareForProcessRemoteChange(const tracked_objects::Location& where,
                                      const base::Closure& closure,
                                      SyncStatusCode expected_status,
                                      SyncFileType expected_file_type,
                                      SyncStatusCode status,
                                      SyncFileType file_type,
                                      const FileChangeList& changes) {
  SCOPED_TRACE(testing::Message() << where.ToString());
  ASSERT_EQ(expected_status, status);
  ASSERT_EQ(expected_file_type, file_type);
  ASSERT_TRUE(changes.empty());
  closure.Run();
}

}  // namespace

class LocalFileSyncServiceTest : public testing::Test {
 protected:
  LocalFileSyncServiceTest()
      : file_thread_(new base::Thread("Thread_File")),
        io_thread_(new base::Thread("Thread_IO")) {}

  ~LocalFileSyncServiceTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_thread_->Start();
    io_thread_->StartWithOptions(
        base::Thread::Options(MessageLoop::TYPE_IO, 0));

    browser_ui_thread_.reset(
        new TestBrowserThread(BrowserThread::UI,
                              MessageLoop::current()));
    browser_file_thread_.reset(
        new TestBrowserThread(BrowserThread::FILE,
                              file_thread_->message_loop()));
    browser_io_thread_.reset(
        new TestBrowserThread(BrowserThread::IO,
                              io_thread_->message_loop()));
    file_system_.reset(new fileapi::CannedSyncableFileSystem(
        GURL(kOrigin), kServiceName,
        io_thread_->message_loop_proxy(),
        file_thread_->message_loop_proxy()));

    local_service_.reset(new LocalFileSyncService);

    file_system_->SetUp();

    base::RunLoop run_loop;
    SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;
    local_service_->MaybeInitializeFileSystemContext(
        GURL(kOrigin), kServiceName, file_system_->file_system_context(),
        AssignAndQuitCallback(&run_loop, &status));
    run_loop.Run();

    EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->OpenFileSystem());
  }

  virtual void TearDown() OVERRIDE {
    local_service_->Shutdown();
    file_system_->TearDown();
    fileapi::RevokeSyncableFileSystem(kServiceName);

    file_thread_->Stop();
    io_thread_->Stop();
  }

  void PrepareForProcessRemoteChange(const FileSystemURL& url,
                                     const tracked_objects::Location& where,
                                     SyncStatusCode expected_status,
                                     SyncFileType expected_file_type) {
    base::RunLoop run_loop;
    local_service_->PrepareForProcessRemoteChange(
        url,
        base::Bind(&DidPrepareForProcessRemoteChange,
                   where,
                   run_loop.QuitClosure(),
                   expected_status,
                   expected_file_type));
    run_loop.Run();
  }

  SyncStatusCode ApplyRemoteChange(const FileChange& change,
                                   const FilePath& local_path,
                                   const FileSystemURL& url) {
    base::RunLoop run_loop;
    SyncStatusCode sync_status = fileapi::SYNC_STATUS_UNKNOWN;
    local_service_->ApplyRemoteChange(
        change, local_path, url,
        AssignAndQuitCallback(&run_loop, &sync_status));
    run_loop.Run();
    return sync_status;
  }

  MessageLoop message_loop_;
  scoped_ptr<base::Thread> file_thread_;
  scoped_ptr<base::Thread> io_thread_;

  scoped_ptr<TestBrowserThread> browser_ui_thread_;
  scoped_ptr<TestBrowserThread> browser_file_thread_;
  scoped_ptr<TestBrowserThread> browser_io_thread_;

  ScopedTempDir temp_dir_;

  scoped_ptr<fileapi::CannedSyncableFileSystem> file_system_;
  scoped_ptr<LocalFileSyncService> local_service_;
};

// More complete tests for PrepareForProcessRemoteChange and ApplyRemoteChange
// are also in content_unittest:LocalFileSyncContextTest.
TEST_F(LocalFileSyncServiceTest, RemoteSyncStepsSimple) {
  const FileSystemURL kFile(file_system_->URL("file"));
  const FileSystemURL kDir(file_system_->URL("dir"));
  const char kTestFileData[] = "0123456789";
  const int kTestFileDataSize = static_cast<int>(arraysize(kTestFileData) - 1);

  FilePath local_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                  &local_path));
  ASSERT_EQ(kTestFileDataSize,
            file_util::WriteFile(local_path, kTestFileData, kTestFileDataSize));

  // Run PrepareForProcessRemoteChange for kFile.
  PrepareForProcessRemoteChange(kFile, FROM_HERE,
                                fileapi::SYNC_STATUS_OK,
                                fileapi::SYNC_FILE_TYPE_UNKNOWN);

  // Run ApplyRemoteChange for kFile.
  FileChange change(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                    fileapi::SYNC_FILE_TYPE_FILE);
  EXPECT_EQ(fileapi::SYNC_STATUS_OK,
            ApplyRemoteChange(change, local_path, kFile));

  // Verify the file is synced.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_->VerifyFile(kFile, kTestFileData));

  // Run PrepareForProcessRemoteChange for kDir.
  PrepareForProcessRemoteChange(kDir, FROM_HERE,
                                fileapi::SYNC_STATUS_OK,
                                fileapi::SYNC_FILE_TYPE_UNKNOWN);

  // Run ApplyRemoteChange for kDir.
  change = FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                      fileapi::SYNC_FILE_TYPE_DIRECTORY);
  EXPECT_EQ(fileapi::SYNC_STATUS_OK,
            ApplyRemoteChange(change, FilePath(), kDir));

  // Verify the directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_->DirectoryExists(kDir));

  // Run ApplyRemoteChange for kDir deletion.
  change = FileChange(FileChange::FILE_CHANGE_DELETE,
                      fileapi::SYNC_FILE_TYPE_UNKNOWN);
  EXPECT_EQ(fileapi::SYNC_STATUS_OK,
            ApplyRemoteChange(change, FilePath(), kDir));

  // Now the directory must have deleted.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            file_system_->DirectoryExists(kDir));
}

}  // namespace sync_file_system
