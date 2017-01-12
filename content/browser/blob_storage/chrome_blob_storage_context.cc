// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/chrome_blob_storage_context.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_memory_controller.h"
#include "storage/browser/blob/blob_storage_context.h"

using base::FilePath;
using base::UserDataAdapter;
using storage::BlobStorageContext;

namespace content {

namespace {
const FilePath::CharType kBlobStorageContextKeyName[] =
    FILE_PATH_LITERAL("content_blob_storage_context");
const FilePath::CharType kBlobStorageParentDirectory[] =
    FILE_PATH_LITERAL("blob_storage");

// Removes all folders in the parent directory except for the
// |current_run_dir| folder. If this path is empty, then we delete all folders.
void RemoveOldBlobStorageDirectories(FilePath blob_storage_parent,
                                     const FilePath& current_run_dir) {
  if (!base::DirectoryExists(blob_storage_parent)) {
    return;
  }
  base::FileEnumerator enumerator(blob_storage_parent, false /* recursive */,
                                  base::FileEnumerator::DIRECTORIES);
  bool success = true;
  bool cleanup_needed = false;
  for (FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    cleanup_needed = true;
    if (current_run_dir.empty() || name != current_run_dir)
      success &= base::DeleteFile(name, true /* recursive */);
  }
  if (cleanup_needed)
    UMA_HISTOGRAM_BOOLEAN("Storage.Blob.CleanupSuccess", success);
}

class BlobHandleImpl : public BlobHandle {
 public:
  explicit BlobHandleImpl(std::unique_ptr<storage::BlobDataHandle> handle)
      : handle_(std::move(handle)) {}

  ~BlobHandleImpl() override {}

  std::string GetUUID() override { return handle_->uuid(); }

 private:
  std::unique_ptr<storage::BlobDataHandle> handle_;
};

}  // namespace

ChromeBlobStorageContext::ChromeBlobStorageContext() {}

ChromeBlobStorageContext* ChromeBlobStorageContext::GetFor(
    BrowserContext* context) {
  if (!context->GetUserData(kBlobStorageContextKeyName)) {
    scoped_refptr<ChromeBlobStorageContext> blob =
        new ChromeBlobStorageContext();
    context->SetUserData(
        kBlobStorageContextKeyName,
        new UserDataAdapter<ChromeBlobStorageContext>(blob.get()));

    // Check first to avoid memory leak in unittests.
    bool io_thread_valid = BrowserThread::IsMessageLoopValid(BrowserThread::IO);

    // Resolve our storage directories.
    FilePath blob_storage_parent =
        context->GetPath().Append(kBlobStorageParentDirectory);
    FilePath blob_storage_dir = blob_storage_parent.Append(
        FilePath::FromUTF8Unsafe(base::GenerateGUID()));

    // Only populate the task runner if we're not off the record. This enables
    // paging/saving blob data to disk.
    scoped_refptr<base::TaskRunner> file_task_runner;

    // If we're not incognito mode, schedule all of our file tasks to enable
    // disk on the storage context.
    if (!context->IsOffTheRecord() && io_thread_valid) {
      file_task_runner =
          BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
      // Removes our old blob directories if they exist.
      BrowserThread::PostAfterStartupTask(
          FROM_HERE, file_task_runner,
          base::Bind(&RemoveOldBlobStorageDirectories,
                     base::Passed(&blob_storage_parent), blob_storage_dir));
    }

    if (io_thread_valid) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&ChromeBlobStorageContext::InitializeOnIOThread, blob,
                     base::Passed(&blob_storage_dir),
                     base::Passed(&file_task_runner)));
    }
  }

  return UserDataAdapter<ChromeBlobStorageContext>::Get(
      context, kBlobStorageContextKeyName);
}

void ChromeBlobStorageContext::InitializeOnIOThread(
    FilePath blob_storage_dir,
    scoped_refptr<base::TaskRunner> file_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_.reset(new BlobStorageContext(std::move(blob_storage_dir),
                                        std::move(file_task_runner)));
  // Signal the BlobMemoryController when it's appropriate to calculate its
  // storage limits.
  BrowserThread::PostAfterStartupTask(
      FROM_HERE, BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
      base::Bind(&storage::BlobMemoryController::CalculateBlobStorageLimits,
                 context_->mutable_memory_controller()->GetWeakPtr()));
}

std::unique_ptr<BlobHandle> ChromeBlobStorageContext::CreateMemoryBackedBlob(
    const char* data,
    size_t length) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::string uuid(base::GenerateGUID());
  storage::BlobDataBuilder blob_data_builder(uuid);
  blob_data_builder.AppendData(data, length);

  std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
      context_->AddFinishedBlob(&blob_data_builder);
  if (!blob_data_handle)
    return std::unique_ptr<BlobHandle>();

  std::unique_ptr<BlobHandle> blob_handle(
      new BlobHandleImpl(std::move(blob_data_handle)));
  return blob_handle;
}

std::unique_ptr<BlobHandle> ChromeBlobStorageContext::CreateFileBackedBlob(
    const FilePath& path,
    int64_t offset,
    int64_t size,
    const base::Time& expected_modification_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::string uuid(base::GenerateGUID());
  storage::BlobDataBuilder blob_data_builder(uuid);
  blob_data_builder.AppendFile(path, offset, size, expected_modification_time);

  std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
      context_->AddFinishedBlob(&blob_data_builder);
  if (!blob_data_handle)
    return std::unique_ptr<BlobHandle>();

  std::unique_ptr<BlobHandle> blob_handle(
      new BlobHandleImpl(std::move(blob_data_handle)));
  return blob_handle;
}

ChromeBlobStorageContext::~ChromeBlobStorageContext() {}

void ChromeBlobStorageContext::DeleteOnCorrectThread() const {
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO) &&
      !BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this);
    return;
  }
  delete this;
}

}  // namespace content
