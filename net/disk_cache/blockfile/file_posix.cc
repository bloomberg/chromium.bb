// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/file.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"

namespace {

// The maximum number of threads for this pool.
const int kMaxThreads = 5;

class FileWorkerPool : public base::SequencedWorkerPool {
 public:
  FileWorkerPool() : base::SequencedWorkerPool(kMaxThreads, "CachePool") {}

 protected:
  virtual ~FileWorkerPool() {}
};

base::LazyInstance<FileWorkerPool>::Leaky s_worker_pool =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace disk_cache {

File::File(base::PlatformFile file)
    : init_(true),
      mixed_(true),
      platform_file_(file),
      sync_platform_file_(base::kInvalidPlatformFileValue) {
}

bool File::Init(const base::FilePath& name) {
  if (init_)
    return false;

  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE;
  platform_file_ = base::CreatePlatformFile(name, flags, NULL, NULL);
  if (platform_file_ < 0) {
    platform_file_ = 0;
    return false;
  }

  init_ = true;
  return true;
}

base::PlatformFile File::platform_file() const {
  return platform_file_;
}

bool File::IsValid() const {
  if (!init_)
    return false;
  return (base::kInvalidPlatformFileValue != platform_file_);
}

bool File::Read(void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > static_cast<size_t>(kint32max) ||
      offset > static_cast<size_t>(kint32max)) {
    return false;
  }

  int ret = base::ReadPlatformFile(platform_file_, offset,
                                   static_cast<char*>(buffer), buffer_len);
  return (static_cast<size_t>(ret) == buffer_len);
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset) {
  DCHECK(init_);
  if (buffer_len > static_cast<size_t>(kint32max) ||
      offset > static_cast<size_t>(kint32max)) {
    return false;
  }

  int ret = base::WritePlatformFile(platform_file_, offset,
                                    static_cast<const char*>(buffer),
                                    buffer_len);
  return (static_cast<size_t>(ret) == buffer_len);
}

bool File::Read(void* buffer, size_t buffer_len, size_t offset,
                FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (!callback) {
    if (completed)
      *completed = true;
    return Read(buffer, buffer_len, offset);
  }

  if (buffer_len > static_cast<size_t>(kint32max) ||
      offset > static_cast<size_t>(kint32max)) {
    return false;
  }

  base::PostTaskAndReplyWithResult(
      s_worker_pool.Pointer(), FROM_HERE,
      base::Bind(&File::DoRead, this, buffer, buffer_len, offset),
      base::Bind(&File::OnOperationComplete, this, callback));

  *completed = false;
  return true;
}

bool File::Write(const void* buffer, size_t buffer_len, size_t offset,
                 FileIOCallback* callback, bool* completed) {
  DCHECK(init_);
  if (!callback) {
    if (completed)
      *completed = true;
    return Write(buffer, buffer_len, offset);
  }

  if (buffer_len > static_cast<size_t>(kint32max) ||
      offset > static_cast<size_t>(kint32max)) {
    return false;
  }

  base::PostTaskAndReplyWithResult(
      s_worker_pool.Pointer(), FROM_HERE,
      base::Bind(&File::DoWrite, this, buffer, buffer_len, offset),
      base::Bind(&File::OnOperationComplete, this, callback));

  *completed = false;
  return true;
}

bool File::SetLength(size_t length) {
  DCHECK(init_);
  if (length > kuint32max)
    return false;

  return base::TruncatePlatformFile(platform_file_, length);
}

size_t File::GetLength() {
  DCHECK(init_);
  int64 len = base::SeekPlatformFile(platform_file_,
                                     base::PLATFORM_FILE_FROM_END, 0);

  if (len > static_cast<int64>(kuint32max))
    return kuint32max;

  return static_cast<size_t>(len);
}

// Static.
void File::WaitForPendingIO(int* num_pending_io) {
  // We are running unit tests so we should wait for all callbacks. Sadly, the
  // worker pool only waits for tasks on the worker pool, not the "Reply" tasks
  // so we have to let the current message loop to run.
  s_worker_pool.Get().FlushForTesting();
  base::RunLoop().RunUntilIdle();
}

// Static.
void File::DropPendingIO() {
}


File::~File() {
  if (IsValid())
    base::ClosePlatformFile(platform_file_);
}

// Runs on a worker thread.
int File::DoRead(void* buffer, size_t buffer_len, size_t offset) {
  if (Read(const_cast<void*>(buffer), buffer_len, offset))
    return static_cast<int>(buffer_len);

  return net::ERR_CACHE_READ_FAILURE;
}

// Runs on a worker thread.
int File::DoWrite(const void* buffer, size_t buffer_len, size_t offset) {
  if (Write(const_cast<void*>(buffer), buffer_len, offset))
    return static_cast<int>(buffer_len);

  return net::ERR_CACHE_WRITE_FAILURE;
}

// This method actually makes sure that the last reference to the file doesn't
// go away on the worker pool.
void File::OnOperationComplete(FileIOCallback* callback, int result) {
  callback->OnFileIOComplete(result);
}

}  // namespace disk_cache
