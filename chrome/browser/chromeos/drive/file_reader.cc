// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_reader.h"

#include <errno.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace drive {
namespace util {

namespace {

// Opens the file at |file_path| and seeks to the |offset| from begin.
// Returns the net::Error code. If succeeded, |platform_file| is set to point
// the opened file.
// This function should run on the blocking pool.
int OpenAndSeekOnBlockingPool(const base::FilePath& file_path,
                              int64 offset,
                              base::PlatformFile* platform_file) {
  DCHECK(platform_file);
  DCHECK_EQ(base::kInvalidPlatformFileValue, *platform_file);

  // First of all, open the file.
  const int open_flags = base::PLATFORM_FILE_OPEN |
                         base::PLATFORM_FILE_READ |
                         base::PLATFORM_FILE_ASYNC;
  base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
  base::PlatformFile file =
      base::CreatePlatformFile(file_path, open_flags, NULL, &error);
  if (file == base::kInvalidPlatformFileValue) {
    DCHECK_NE(base::PLATFORM_FILE_OK, error);
    return net::PlatformFileErrorToNetError(error);
  }

  // If succeeded, seek to the |offset| from begin.
  int64 position = base::SeekPlatformFile(
      file, base::PLATFORM_FILE_FROM_BEGIN, offset);
  if (position < 0) {
    // If failed, close the file and return an error.
    base::ClosePlatformFile(file);
    return net::ERR_FAILED;
  }

  *platform_file = file;
  return net::OK;
}

// Reads the data from the |platform_file| and copies it to the |buffer| at
// most |buffer_length| size. Returns the number of copied bytes if succeeded,
// or the net::Error code.
// This function should run on the blocking pool.
int ReadOnBlockingPool(base::PlatformFile platform_file,
                       scoped_refptr<net::IOBuffer> buffer,
                       int buffer_length) {
  DCHECK_NE(base::kInvalidPlatformFileValue, platform_file);
  int result = base::ReadPlatformFileCurPosNoBestEffort(
      platform_file, buffer->data(), buffer_length);
  return result < 0 ? net::MapSystemError(errno) : result;
}

// Posts a task to close the |platform_file| into |task_runner|.
// Or, if |platform_file| is kInvalidPlatformFileValue, does nothing.
void PostCloseIfNeeded(base::TaskRunner* task_runner,
                       base::PlatformFile platform_file) {
  DCHECK(task_runner);
  if (platform_file != base::kInvalidPlatformFileValue) {
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(
            base::IgnoreResult(&base::ClosePlatformFile), platform_file));
  }
}

}  // namespace

class FileReader::ScopedPlatformFile {
 public:
  explicit ScopedPlatformFile(base::TaskRunner* task_runner)
      : task_runner_(task_runner),
        platform_file_(base::kInvalidPlatformFileValue) {
    DCHECK(task_runner);
  }

  ~ScopedPlatformFile() {
    PostCloseIfNeeded(task_runner_.get(), platform_file_);
  }

  base::PlatformFile* ptr() { return &platform_file_; }

  base::PlatformFile release() {
    base::PlatformFile result = platform_file_;
    platform_file_ = base::kInvalidPlatformFileValue;
    return result;
  }

 private:
  scoped_refptr<base::TaskRunner> task_runner_;
  base::PlatformFile platform_file_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPlatformFile);
};

FileReader::FileReader(base::SequencedTaskRunner* sequenced_task_runner)
    : sequenced_task_runner_(sequenced_task_runner),
      platform_file_(base::kInvalidPlatformFileValue),
      weak_ptr_factory_(this) {
  DCHECK(sequenced_task_runner_);
}

FileReader::~FileReader() {
  PostCloseIfNeeded(sequenced_task_runner_.get(), platform_file_);
}

void FileReader::Open(const base::FilePath& file_path,
                      int64 offset,
                      const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK_EQ(base::kInvalidPlatformFileValue, platform_file_);

  ScopedPlatformFile* platform_file =
      new ScopedPlatformFile(sequenced_task_runner_);
  base::PostTaskAndReplyWithResult(
      sequenced_task_runner_,
      FROM_HERE,
      base::Bind(&OpenAndSeekOnBlockingPool,
                 file_path, offset, platform_file->ptr()),
      base::Bind(&FileReader::OpenAfterBlockingPoolTask,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback, base::Owned(platform_file)));
}

void FileReader::Read(net::IOBuffer* in_buffer,
                      int buffer_length,
                      const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK_NE(base::kInvalidPlatformFileValue, platform_file_);

  scoped_refptr<net::IOBuffer> buffer(in_buffer);
  base::PostTaskAndReplyWithResult(
      sequenced_task_runner_,
      FROM_HERE,
      base::Bind(&ReadOnBlockingPool, platform_file_, buffer, buffer_length),
      callback);
}

void FileReader::OpenAfterBlockingPoolTask(
    const net::CompletionCallback& callback,
    ScopedPlatformFile* platform_file,
    int open_result) {
  DCHECK(!callback.is_null());
  DCHECK(platform_file);
  DCHECK_EQ(base::kInvalidPlatformFileValue, platform_file_);

  if (open_result == net::OK) {
    DCHECK_NE(base::kInvalidPlatformFileValue, *platform_file->ptr());
    platform_file_ = platform_file->release();
  }

  callback.Run(open_result);
}

}  // namespace util
}  // namespace drive
