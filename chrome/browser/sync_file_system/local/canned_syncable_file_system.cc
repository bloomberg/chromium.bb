// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/canned_syncable_file_system.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/test/mock_blob_url_request_context.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_file_system_options.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/blob/shareable_file_reference.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::File;
using storage::FileSystemContext;
using storage::FileSystemOperationRunner;
using storage::FileSystemURL;
using storage::FileSystemURLSet;
using storage::QuotaManager;
using content::MockBlobURLRequestContext;
using content::ScopedTextBlob;

namespace sync_file_system {

namespace {

template <typename R>
void AssignAndQuit(base::TaskRunner* original_task_runner,
                   const base::Closure& quit_closure,
                   R* result_out, R result) {
  DCHECK(result_out);
  *result_out = result;
  original_task_runner->PostTask(FROM_HERE, quit_closure);
}

template <typename R>
R RunOnThread(
    base::SingleThreadTaskRunner* task_runner,
    const tracked_objects::Location& location,
    const base::Callback<void(const base::Callback<void(R)>& callback)>& task) {
  R result;
  base::RunLoop run_loop;
  task_runner->PostTask(
      location,
      base::Bind(task, base::Bind(&AssignAndQuit<R>,
                                  base::ThreadTaskRunnerHandle::Get(),
                                  run_loop.QuitClosure(),
                                  &result)));
  run_loop.Run();
  return result;
}

void RunOnThread(base::SingleThreadTaskRunner* task_runner,
                 const tracked_objects::Location& location,
                 const base::Closure& task) {
  base::RunLoop run_loop;
  task_runner->PostTaskAndReply(
      location, task,
      base::Bind(base::IgnoreResult(
          base::Bind(&base::SingleThreadTaskRunner::PostTask,
                     base::ThreadTaskRunnerHandle::Get(),
                     FROM_HERE, run_loop.QuitClosure()))));
  run_loop.Run();
}

void EnsureRunningOn(base::SingleThreadTaskRunner* runner) {
  EXPECT_TRUE(runner->RunsTasksOnCurrentThread());
}

void VerifySameTaskRunner(
    base::SingleThreadTaskRunner* runner1,
    base::SingleThreadTaskRunner* runner2) {
  ASSERT_TRUE(runner1 != NULL);
  ASSERT_TRUE(runner2 != NULL);
  runner1->PostTask(FROM_HERE,
                    base::Bind(&EnsureRunningOn, make_scoped_refptr(runner2)));
}

void OnCreateSnapshotFileAndVerifyData(
    const std::string& expected_data,
    const CannedSyncableFileSystem::StatusCallback& callback,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    const scoped_refptr<storage::ShareableFileReference>& /* file_ref */) {
  if (result != base::File::FILE_OK) {
    callback.Run(result);
    return;
  }
  EXPECT_EQ(expected_data.size(), static_cast<size_t>(file_info.size));
  std::string data;
  const bool read_status = base::ReadFileToString(platform_path, &data);
  EXPECT_TRUE(read_status);
  EXPECT_EQ(expected_data, data);
  callback.Run(result);
}

void OnCreateSnapshotFile(
    base::File::Info* file_info_out,
    base::FilePath* platform_path_out,
    const CannedSyncableFileSystem::StatusCallback& callback,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    const scoped_refptr<storage::ShareableFileReference>& file_ref) {
  DCHECK(!file_ref.get());
  DCHECK(file_info_out);
  DCHECK(platform_path_out);
  *file_info_out = file_info;
  *platform_path_out = platform_path;
  callback.Run(result);
}

void OnReadDirectory(CannedSyncableFileSystem::FileEntryList* entries_out,
                     const CannedSyncableFileSystem::StatusCallback& callback,
                     base::File::Error error,
                     const storage::FileSystemOperation::FileEntryList& entries,
                     bool has_more) {
  DCHECK(entries_out);
  entries_out->reserve(entries_out->size() + entries.size());
  std::copy(entries.begin(), entries.end(), std::back_inserter(*entries_out));

  if (!has_more)
    callback.Run(error);
}

class WriteHelper {
 public:
  WriteHelper() : bytes_written_(0) {}
  WriteHelper(MockBlobURLRequestContext* request_context,
              const std::string& blob_data)
      : bytes_written_(0),
        request_context_(request_context),
        blob_data_(new ScopedTextBlob(*request_context,
                                      base::GenerateGUID(),
                                      blob_data)) {
  }

  ~WriteHelper() {
    if (request_context_) {
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
          FROM_HERE, request_context_.release());
    }
  }

  ScopedTextBlob* scoped_text_blob() const { return blob_data_.get(); }

  void DidWrite(const base::Callback<void(int64 result)>& completion_callback,
                File::Error error, int64 bytes, bool complete) {
    if (error == base::File::FILE_OK) {
      bytes_written_ += bytes;
      if (!complete)
        return;
    }
    completion_callback.Run(error == base::File::FILE_OK ?
                            bytes_written_ : static_cast<int64>(error));
  }

 private:
  int64 bytes_written_;
  scoped_ptr<MockBlobURLRequestContext> request_context_;
  scoped_ptr<ScopedTextBlob> blob_data_;

  DISALLOW_COPY_AND_ASSIGN(WriteHelper);
};

void DidGetUsageAndQuota(const storage::StatusCallback& callback,
                         int64* usage_out,
                         int64* quota_out,
                         storage::QuotaStatusCode status,
                         int64 usage,
                         int64 quota) {
  *usage_out = usage;
  *quota_out = quota;
  callback.Run(status);
}

void EnsureLastTaskRuns(base::SingleThreadTaskRunner* runner) {
  base::RunLoop run_loop;
  runner->PostTaskAndReply(
      FROM_HERE, base::Bind(&base::DoNothing), run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

CannedSyncableFileSystem::CannedSyncableFileSystem(
    const GURL& origin,
    leveldb::Env* env_override,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner)
    : origin_(origin),
      type_(storage::kFileSystemTypeSyncable),
      result_(base::File::FILE_OK),
      sync_status_(sync_file_system::SYNC_STATUS_OK),
      env_override_(env_override),
      io_task_runner_(io_task_runner),
      file_task_runner_(file_task_runner),
      is_filesystem_set_up_(false),
      is_filesystem_opened_(false),
      sync_status_observers_(new ObserverList) {
}

CannedSyncableFileSystem::~CannedSyncableFileSystem() {}

void CannedSyncableFileSystem::SetUp(QuotaMode quota_mode) {
  ASSERT_FALSE(is_filesystem_set_up_);
  ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

  scoped_refptr<storage::SpecialStoragePolicy> storage_policy =
      new content::MockSpecialStoragePolicy();

  if (quota_mode == QUOTA_ENABLED) {
    quota_manager_ = new QuotaManager(false /* is_incognito */,
                                      data_dir_.path(),
                                      io_task_runner_.get(),
                                      base::ThreadTaskRunnerHandle::Get().get(),
                                      storage_policy.get());
  }

  std::vector<std::string> additional_allowed_schemes;
  additional_allowed_schemes.push_back(origin_.scheme());
  storage::FileSystemOptions options(
      storage::FileSystemOptions::PROFILE_MODE_NORMAL,
      additional_allowed_schemes,
      env_override_);

  ScopedVector<storage::FileSystemBackend> additional_backends;
  additional_backends.push_back(SyncFileSystemBackend::CreateForTesting());

  file_system_context_ = new FileSystemContext(
      io_task_runner_.get(),
      file_task_runner_.get(),
      storage::ExternalMountPoints::CreateRefCounted().get(),
      storage_policy.get(),
      quota_manager_.get() ? quota_manager_->proxy() : NULL,
      additional_backends.Pass(),
      std::vector<storage::URLRequestAutoMountHandler>(),
      data_dir_.path(),
      options);

  is_filesystem_set_up_ = true;
}

void CannedSyncableFileSystem::TearDown() {
  quota_manager_ = NULL;
  file_system_context_ = NULL;

  // Make sure we give some more time to finish tasks on other threads.
  EnsureLastTaskRuns(io_task_runner_.get());
  EnsureLastTaskRuns(file_task_runner_.get());
}

FileSystemURL CannedSyncableFileSystem::URL(const std::string& path) const {
  EXPECT_TRUE(is_filesystem_set_up_);
  EXPECT_FALSE(root_url_.is_empty());

  GURL url(root_url_.spec() + path);
  return file_system_context_->CrackURL(url);
}

File::Error CannedSyncableFileSystem::OpenFileSystem() {
  EXPECT_TRUE(is_filesystem_set_up_);

  base::RunLoop run_loop;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoOpenFileSystem,
                 base::Unretained(this),
                 base::Bind(&CannedSyncableFileSystem::DidOpenFileSystem,
                            base::Unretained(this),
                            base::ThreadTaskRunnerHandle::Get(),
                            run_loop.QuitClosure())));
  run_loop.Run();

  if (backend()->sync_context()) {
    // Register 'this' as a sync status observer.
    RunOnThread(
        io_task_runner_.get(),
        FROM_HERE,
        base::Bind(&CannedSyncableFileSystem::InitializeSyncStatusObserver,
                   base::Unretained(this)));
  }
  return result_;
}

void CannedSyncableFileSystem::AddSyncStatusObserver(
    LocalFileSyncStatus::Observer* observer) {
  sync_status_observers_->AddObserver(observer);
}

void CannedSyncableFileSystem::RemoveSyncStatusObserver(
    LocalFileSyncStatus::Observer* observer) {
  sync_status_observers_->RemoveObserver(observer);
}

SyncStatusCode CannedSyncableFileSystem::MaybeInitializeFileSystemContext(
    LocalFileSyncContext* sync_context) {
  DCHECK(sync_context);
  base::RunLoop run_loop;
  sync_status_ = sync_file_system::SYNC_STATUS_UNKNOWN;
  VerifySameTaskRunner(io_task_runner_.get(),
                       sync_context->io_task_runner_.get());
  sync_context->MaybeInitializeFileSystemContext(
      origin_,
      file_system_context_.get(),
      base::Bind(&CannedSyncableFileSystem::DidInitializeFileSystemContext,
                 base::Unretained(this),
                 run_loop.QuitClosure()));
  run_loop.Run();
  return sync_status_;
}

File::Error CannedSyncableFileSystem::CreateDirectory(
    const FileSystemURL& url) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoCreateDirectory,
                 base::Unretained(this),
                 url));
}

File::Error CannedSyncableFileSystem::CreateFile(const FileSystemURL& url) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoCreateFile,
                 base::Unretained(this),
                 url));
}

File::Error CannedSyncableFileSystem::Copy(
    const FileSystemURL& src_url, const FileSystemURL& dest_url) {
  return RunOnThread<File::Error>(io_task_runner_.get(),
                                  FROM_HERE,
                                  base::Bind(&CannedSyncableFileSystem::DoCopy,
                                             base::Unretained(this),
                                             src_url,
                                             dest_url));
}

File::Error CannedSyncableFileSystem::Move(
    const FileSystemURL& src_url, const FileSystemURL& dest_url) {
  return RunOnThread<File::Error>(io_task_runner_.get(),
                                  FROM_HERE,
                                  base::Bind(&CannedSyncableFileSystem::DoMove,
                                             base::Unretained(this),
                                             src_url,
                                             dest_url));
}

File::Error CannedSyncableFileSystem::TruncateFile(
    const FileSystemURL& url, int64 size) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoTruncateFile,
                 base::Unretained(this),
                 url,
                 size));
}

File::Error CannedSyncableFileSystem::TouchFile(
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoTouchFile,
                 base::Unretained(this),
                 url,
                 last_access_time,
                 last_modified_time));
}

File::Error CannedSyncableFileSystem::Remove(
    const FileSystemURL& url, bool recursive) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoRemove,
                 base::Unretained(this),
                 url,
                 recursive));
}

File::Error CannedSyncableFileSystem::FileExists(
    const FileSystemURL& url) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoFileExists,
                 base::Unretained(this),
                 url));
}

File::Error CannedSyncableFileSystem::DirectoryExists(
    const FileSystemURL& url) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoDirectoryExists,
                 base::Unretained(this),
                 url));
}

File::Error CannedSyncableFileSystem::VerifyFile(
    const FileSystemURL& url,
    const std::string& expected_data) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoVerifyFile,
                 base::Unretained(this),
                 url,
                 expected_data));
}

File::Error CannedSyncableFileSystem::GetMetadataAndPlatformPath(
    const FileSystemURL& url,
    base::File::Info* info,
    base::FilePath* platform_path) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoGetMetadataAndPlatformPath,
                 base::Unretained(this),
                 url,
                 info,
                 platform_path));
}

File::Error CannedSyncableFileSystem::ReadDirectory(
    const storage::FileSystemURL& url,
    FileEntryList* entries) {
  return RunOnThread<File::Error>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoReadDirectory,
                 base::Unretained(this),
                 url,
                 entries));
}

int64 CannedSyncableFileSystem::Write(
    net::URLRequestContext* url_request_context,
    const FileSystemURL& url,
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  return RunOnThread<int64>(io_task_runner_.get(),
                            FROM_HERE,
                            base::Bind(&CannedSyncableFileSystem::DoWrite,
                                       base::Unretained(this),
                                       url_request_context,
                                       url,
                                       base::Passed(&blob_data_handle)));
}

int64 CannedSyncableFileSystem::WriteString(
    const FileSystemURL& url, const std::string& data) {
  return RunOnThread<int64>(io_task_runner_.get(),
                            FROM_HERE,
                            base::Bind(&CannedSyncableFileSystem::DoWriteString,
                                       base::Unretained(this),
                                       url,
                                       data));
}

File::Error CannedSyncableFileSystem::DeleteFileSystem() {
  EXPECT_TRUE(is_filesystem_set_up_);
  return RunOnThread<File::Error>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FileSystemContext::DeleteFileSystem,
                 file_system_context_,
                 origin_,
                 type_));
}

storage::QuotaStatusCode CannedSyncableFileSystem::GetUsageAndQuota(
    int64* usage,
    int64* quota) {
  return RunOnThread<storage::QuotaStatusCode>(
      io_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoGetUsageAndQuota,
                 base::Unretained(this),
                 usage,
                 quota));
}

void CannedSyncableFileSystem::GetChangedURLsInTracker(
    FileSystemURLSet* urls) {
  RunOnThread(file_task_runner_.get(),
              FROM_HERE,
              base::Bind(&LocalFileChangeTracker::GetAllChangedURLs,
                         base::Unretained(backend()->change_tracker()),
                         urls));
}

void CannedSyncableFileSystem::ClearChangeForURLInTracker(
    const FileSystemURL& url) {
  RunOnThread(file_task_runner_.get(),
              FROM_HERE,
              base::Bind(&LocalFileChangeTracker::ClearChangesForURL,
                         base::Unretained(backend()->change_tracker()),
                         url));
}

void CannedSyncableFileSystem::GetChangesForURLInTracker(
    const FileSystemURL& url,
    FileChangeList* changes) {
  RunOnThread(file_task_runner_.get(),
              FROM_HERE,
              base::Bind(&LocalFileChangeTracker::GetChangesForURL,
                         base::Unretained(backend()->change_tracker()),
                         url,
                         changes));
}

SyncFileSystemBackend* CannedSyncableFileSystem::backend() {
  return SyncFileSystemBackend::GetBackend(file_system_context_.get());
}

FileSystemOperationRunner* CannedSyncableFileSystem::operation_runner() {
  return file_system_context_->operation_runner();
}

void CannedSyncableFileSystem::OnSyncEnabled(const FileSystemURL& url) {
  sync_status_observers_->Notify(&LocalFileSyncStatus::Observer::OnSyncEnabled,
                                 url);
}

void CannedSyncableFileSystem::OnWriteEnabled(const FileSystemURL& url) {
  sync_status_observers_->Notify(&LocalFileSyncStatus::Observer::OnWriteEnabled,
                                 url);
}

void CannedSyncableFileSystem::DoOpenFileSystem(
    const OpenFileSystemCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_FALSE(is_filesystem_opened_);
  file_system_context_->OpenFileSystem(
      origin_,
      type_,
      storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      callback);
}

void CannedSyncableFileSystem::DoCreateDirectory(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->CreateDirectory(
      url, false /* exclusive */, false /* recursive */, callback);
}

void CannedSyncableFileSystem::DoCreateFile(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->CreateFile(url, false /* exclusive */, callback);
}

void CannedSyncableFileSystem::DoCopy(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->Copy(
      src_url,
      dest_url,
      storage::FileSystemOperation::OPTION_NONE,
      storage::FileSystemOperationRunner::CopyProgressCallback(),
      callback);
}

void CannedSyncableFileSystem::DoMove(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->Move(
      src_url, dest_url, storage::FileSystemOperation::OPTION_NONE, callback);
}

void CannedSyncableFileSystem::DoTruncateFile(
    const FileSystemURL& url, int64 size,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->Truncate(url, size, callback);
}

void CannedSyncableFileSystem::DoTouchFile(
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->TouchFile(url, last_access_time,
                                last_modified_time, callback);
}

void CannedSyncableFileSystem::DoRemove(
    const FileSystemURL& url, bool recursive,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->Remove(url, recursive, callback);
}

void CannedSyncableFileSystem::DoFileExists(
    const FileSystemURL& url, const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->FileExists(url, callback);
}

void CannedSyncableFileSystem::DoDirectoryExists(
    const FileSystemURL& url, const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->DirectoryExists(url, callback);
}

void CannedSyncableFileSystem::DoVerifyFile(
    const FileSystemURL& url,
    const std::string& expected_data,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->CreateSnapshotFile(
      url,
      base::Bind(&OnCreateSnapshotFileAndVerifyData, expected_data, callback));
}

void CannedSyncableFileSystem::DoGetMetadataAndPlatformPath(
    const FileSystemURL& url,
    base::File::Info* info,
    base::FilePath* platform_path,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->CreateSnapshotFile(
      url, base::Bind(&OnCreateSnapshotFile, info, platform_path, callback));
}

void CannedSyncableFileSystem::DoReadDirectory(
    const FileSystemURL& url,
    FileEntryList* entries,
    const StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  operation_runner()->ReadDirectory(
      url, base::Bind(&OnReadDirectory, entries, callback));
}

void CannedSyncableFileSystem::DoWrite(
    net::URLRequestContext* url_request_context,
    const FileSystemURL& url,
    scoped_ptr<storage::BlobDataHandle> blob_data_handle,
    const WriteCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  WriteHelper* helper = new WriteHelper;
  operation_runner()->Write(url_request_context, url,
                            blob_data_handle.Pass(), 0,
                            base::Bind(&WriteHelper::DidWrite,
                                       base::Owned(helper), callback));
}

void CannedSyncableFileSystem::DoWriteString(
    const FileSystemURL& url,
    const std::string& data,
    const WriteCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  MockBlobURLRequestContext* url_request_context(
      new MockBlobURLRequestContext(file_system_context_.get()));
  WriteHelper* helper = new WriteHelper(url_request_context, data);
  operation_runner()->Write(url_request_context, url,
                            helper->scoped_text_blob()->GetBlobDataHandle(), 0,
                            base::Bind(&WriteHelper::DidWrite,
                                       base::Owned(helper), callback));
}

void CannedSyncableFileSystem::DoGetUsageAndQuota(
    int64* usage,
    int64* quota,
    const storage::StatusCallback& callback) {
  EXPECT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  EXPECT_TRUE(is_filesystem_opened_);
  DCHECK(quota_manager_.get());
  quota_manager_->GetUsageAndQuota(
      origin_, storage_type(),
      base::Bind(&DidGetUsageAndQuota, callback, usage, quota));
}

void CannedSyncableFileSystem::DidOpenFileSystem(
    base::SingleThreadTaskRunner* original_task_runner,
    const base::Closure& quit_closure,
    const GURL& root,
    const std::string& name,
    File::Error result) {
  if (io_task_runner_->RunsTasksOnCurrentThread()) {
    EXPECT_FALSE(is_filesystem_opened_);
    is_filesystem_opened_ = true;
  }
  if (!original_task_runner->RunsTasksOnCurrentThread()) {
    DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
    original_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&CannedSyncableFileSystem::DidOpenFileSystem,
                   base::Unretained(this),
                   make_scoped_refptr(original_task_runner),
                   quit_closure,
                   root, name, result));
    return;
  }
  result_ = result;
  root_url_ = root;
  quit_closure.Run();
}

void CannedSyncableFileSystem::DidInitializeFileSystemContext(
    const base::Closure& quit_closure,
    SyncStatusCode status) {
  sync_status_ = status;
  quit_closure.Run();
}

void CannedSyncableFileSystem::InitializeSyncStatusObserver() {
  ASSERT_TRUE(io_task_runner_->RunsTasksOnCurrentThread());
  backend()->sync_context()->sync_status()->AddObserver(this);
}

}  // namespace sync_file_system
