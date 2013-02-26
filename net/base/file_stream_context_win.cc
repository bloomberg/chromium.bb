// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_context.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

// Ensure that we can just use our Whence values directly.
COMPILE_ASSERT(FROM_BEGIN == FILE_BEGIN, bad_whence_begin);
COMPILE_ASSERT(FROM_CURRENT == FILE_CURRENT, bad_whence_current);
COMPILE_ASSERT(FROM_END == FILE_END, bad_whence_end);

namespace {

void SetOffset(OVERLAPPED* overlapped, const LARGE_INTEGER& offset) {
  overlapped->Offset = offset.LowPart;
  overlapped->OffsetHigh = offset.HighPart;
}

void IncrementOffset(OVERLAPPED* overlapped, DWORD count) {
  LARGE_INTEGER offset;
  offset.LowPart = overlapped->Offset;
  offset.HighPart = overlapped->OffsetHigh;
  offset.QuadPart += static_cast<LONGLONG>(count);
  SetOffset(overlapped, offset);
}

}  // namespace

FileStream::Context::Context(const BoundNetLog& bound_net_log)
    : io_context_(),
      file_(base::kInvalidPlatformFileValue),
      record_uma_(false),
      async_in_progress_(false),
      orphaned_(false),
      bound_net_log_(bound_net_log),
      error_source_(FILE_ERROR_SOURCE_COUNT) {
  io_context_.handler = this;
  memset(&io_context_.overlapped, 0, sizeof(io_context_.overlapped));
}

FileStream::Context::Context(base::PlatformFile file,
                             const BoundNetLog& bound_net_log,
                             int open_flags)
    : io_context_(),
      file_(file),
      record_uma_(false),
      async_in_progress_(false),
      orphaned_(false),
      bound_net_log_(bound_net_log),
      error_source_(FILE_ERROR_SOURCE_COUNT) {
  io_context_.handler = this;
  memset(&io_context_.overlapped, 0, sizeof(io_context_.overlapped));
  if (file_ != base::kInvalidPlatformFileValue &&
      (open_flags & base::PLATFORM_FILE_ASYNC)) {
    OnAsyncFileOpened();
  }
}

FileStream::Context::~Context() {
}

int64 FileStream::Context::GetFileSize() const {
  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(file_, &file_size)) {
    IOResult error = IOResult::FromOSError(GetLastError());
    LOG(WARNING) << "GetFileSizeEx failed: " << error.os_error;
    RecordError(error, FILE_ERROR_SOURCE_GET_SIZE);
    return error.result;
  }

  return file_size.QuadPart;
}

int FileStream::Context::ReadAsync(IOBuffer* buf,
                                   int buf_len,
                                   const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);
  error_source_ = FILE_ERROR_SOURCE_READ;

  DWORD bytes_read;
  if (!ReadFile(file_, buf->data(), buf_len,
                &bytes_read, &io_context_.overlapped)) {
    IOResult error = IOResult::FromOSError(GetLastError());
    if (error.os_error == ERROR_IO_PENDING) {
      IOCompletionIsPending(callback, buf);
    } else if (error.os_error == ERROR_HANDLE_EOF) {
      return 0;  // Report EOF by returning 0 bytes read.
    } else {
      LOG(WARNING) << "ReadFile failed: " << error.os_error;
      RecordError(error, FILE_ERROR_SOURCE_READ);
    }
    return error.result;
  }

  IOCompletionIsPending(callback, buf);
  return ERR_IO_PENDING;
}

int FileStream::Context::ReadSync(char* buf, int buf_len) {
  DWORD bytes_read;
  if (!ReadFile(file_, buf, buf_len, &bytes_read, NULL)) {
    IOResult error = IOResult::FromOSError(GetLastError());
    if (error.os_error == ERROR_HANDLE_EOF) {
      return 0;  // Report EOF by returning 0 bytes read.
    } else {
      LOG(WARNING) << "ReadFile failed: " << error.os_error;
      RecordError(error, FILE_ERROR_SOURCE_READ);
    }
    return error.result;
  }

  return bytes_read;
}

int FileStream::Context::WriteAsync(IOBuffer* buf,
                                    int buf_len,
                                    const CompletionCallback& callback) {
  error_source_ = FILE_ERROR_SOURCE_WRITE;

  DWORD bytes_written = 0;
  if (!WriteFile(file_, buf->data(), buf_len,
                 &bytes_written, &io_context_.overlapped)) {
    IOResult error = IOResult::FromOSError(GetLastError());
    if (error.os_error == ERROR_IO_PENDING) {
      IOCompletionIsPending(callback, buf);
    } else {
      LOG(WARNING) << "WriteFile failed: " << error.os_error;
      RecordError(error, FILE_ERROR_SOURCE_WRITE);
    }
    return error.result;
  }

  IOCompletionIsPending(callback, buf);
  return ERR_IO_PENDING;
}

int FileStream::Context::WriteSync(const char* buf, int buf_len) {
  DWORD bytes_written = 0;
  if (!WriteFile(file_, buf, buf_len, &bytes_written, NULL)) {
    IOResult error = IOResult::FromOSError(GetLastError());
    LOG(WARNING) << "WriteFile failed: " << error.os_error;
    RecordError(error, FILE_ERROR_SOURCE_WRITE);
    return error.result;
  }

  return bytes_written;
}

int FileStream::Context::Truncate(int64 bytes) {
  if (!SetEndOfFile(file_)) {
    IOResult error = IOResult::FromOSError(GetLastError());
    LOG(WARNING) << "SetEndOfFile failed: " << error.os_error;
    RecordError(error, FILE_ERROR_SOURCE_SET_EOF);
    return error.result;
  }

  return bytes;
}

void FileStream::Context::OnAsyncFileOpened() {
  MessageLoopForIO::current()->RegisterIOHandler(file_, this);
}

FileStream::Context::IOResult FileStream::Context::SeekFileImpl(Whence whence,
                                                                int64 offset) {
  LARGE_INTEGER distance, res;
  distance.QuadPart = offset;
  DWORD move_method = static_cast<DWORD>(whence);
  if (SetFilePointerEx(file_, distance, &res, move_method)) {
    SetOffset(&io_context_.overlapped, res);
    return IOResult(res.QuadPart, 0);
  }

  return IOResult::FromOSError(GetLastError());
}

FileStream::Context::IOResult FileStream::Context::FlushFileImpl() {
  if (FlushFileBuffers(file_))
    return IOResult(OK, 0);

  return IOResult::FromOSError(GetLastError());
}

void FileStream::Context::IOCompletionIsPending(
    const CompletionCallback& callback,
    IOBuffer* buf) {
  DCHECK(callback_.is_null());
  callback_ = callback;
  in_flight_buf_ = buf;  // Hold until the async operation ends.
  async_in_progress_ = true;
}

void FileStream::Context::OnIOCompleted(MessageLoopForIO::IOContext* context,
                                        DWORD bytes_read,
                                        DWORD error) {
  DCHECK_EQ(&io_context_, context);
  DCHECK(!callback_.is_null());
  DCHECK(async_in_progress_);

  async_in_progress_ = false;
  if (orphaned_) {
    callback_.Reset();
    in_flight_buf_ = NULL;
    CloseAndDelete();
    return;
  }

  int result;
  if (error == ERROR_HANDLE_EOF) {
    result = 0;
  } else if (error) {
    IOResult error_result = IOResult::FromOSError(error);
    RecordError(error_result, error_source_);
    result = error_result.result;
  } else {
    result = bytes_read;
    IncrementOffset(&io_context_.overlapped, bytes_read);
  }

  CompletionCallback temp_callback = callback_;
  callback_.Reset();
  scoped_refptr<IOBuffer> temp_buf = in_flight_buf_;
  in_flight_buf_ = NULL;
  temp_callback.Run(result);
}

}  // namespace net
