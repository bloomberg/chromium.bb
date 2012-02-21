// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For 64-bit file access (off_t = off64_t, lseek64, etc).
#define _FILE_OFFSET_BITS 64

#include "net/base/file_stream.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/file_stream_metrics.h"
#include "net/base/file_stream_net_log_parameters.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

#if defined(OS_ANDROID)
// Android's bionic libc only supports the LFS transitional API.
#define off_t off64_t
#define lseek lseek64
#define stat stat64
#define fstat fstat64
#endif

namespace net {

// We cast back and forth, so make sure it's the size we're expecting.
COMPILE_ASSERT(sizeof(int64) == sizeof(off_t), off_t_64_bit);

// Make sure our Whence mappings match the system headers.
COMPILE_ASSERT(FROM_BEGIN   == SEEK_SET &&
               FROM_CURRENT == SEEK_CUR &&
               FROM_END     == SEEK_END, whence_matches_system);

namespace {

int RecordAndMapError(int error,
                      FileErrorSource source,
                      bool record_uma,
                      const net::BoundNetLog& bound_net_log) {
  net::Error net_error = MapSystemError(error);

  bound_net_log.AddEvent(
      net::NetLog::TYPE_FILE_STREAM_ERROR,
      make_scoped_refptr(
          new FileStreamErrorParameters(GetFileErrorSourceName(source),
                                        error,
                                        net_error)));

  RecordFileError(error, source, record_uma);

  return net_error;
}

// Opens a file with some network logging.
// The opened file and the result code are written to |file| and |result|.
void OpenFile(const FilePath& path,
              int open_flags,
              bool record_uma,
              base::PlatformFile* file,
              int* result,
              const net::BoundNetLog& bound_net_log) {
  bound_net_log.BeginEvent(
      net::NetLog::TYPE_FILE_STREAM_OPEN,
      make_scoped_refptr(
          new net::NetLogStringParameter("file_name",
                                         path.AsUTF8Unsafe())));

  *result = OK;
  *file = base::CreatePlatformFile(path, open_flags, NULL, NULL);
  if (*file == base::kInvalidPlatformFileValue) {
    bound_net_log.EndEvent(net::NetLog::TYPE_FILE_STREAM_OPEN, NULL);
    *result = RecordAndMapError(errno, FILE_ERROR_SOURCE_OPEN, record_uma,
                                bound_net_log);
  }
}

// Closes a file with some network logging.
void CloseFile(base::PlatformFile file,
               const net::BoundNetLog& bound_net_log) {
  bound_net_log.AddEvent(net::NetLog::TYPE_FILE_STREAM_CLOSE, NULL);
  if (file == base::kInvalidPlatformFileValue)
    return;

  if (!base::ClosePlatformFile(file))
    NOTREACHED();
  bound_net_log.EndEvent(net::NetLog::TYPE_FILE_STREAM_OPEN, NULL);
}

// ReadFile() is a simple wrapper around read() that handles EINTR signals and
// calls MapSystemError() to map errno to net error codes.
void ReadFile(base::PlatformFile file,
              char* buf,
              int buf_len,
              bool record_uma,
              int* result,
              const net::BoundNetLog& bound_net_log) {
  base::ThreadRestrictions::AssertIOAllowed();
  // read(..., 0) returns 0 to indicate end-of-file.

  // Loop in the case of getting interrupted by a signal.
  ssize_t res = HANDLE_EINTR(read(file, buf, static_cast<size_t>(buf_len)));
  if (res == -1) {
    res = RecordAndMapError(errno, FILE_ERROR_SOURCE_READ,
                            record_uma, bound_net_log);
  }
  *result = res;
}

// Helper function used for FileStream::Read().
void ReadFileFromIOBuffer(base::PlatformFile file,
                          scoped_refptr<IOBuffer> buf,
                          int buf_len,
                          bool record_uma,
                          int* result,
                          base::WaitableEvent* on_complete,
                          const net::BoundNetLog& bound_net_log) {
  ReadFile(file, buf->data(), buf_len, record_uma, result, bound_net_log);
  on_complete->Signal();
}

// WriteFile() is a simple wrapper around write() that handles EINTR signals and
// calls MapSystemError() to map errno to net error codes.  It tries to write to
// completion.
void WriteFile(base::PlatformFile file,
               const char* buf,
               int buf_len,
               bool record_uma,
               int* result,
               const net::BoundNetLog& bound_net_log) {
  base::ThreadRestrictions::AssertIOAllowed();

  ssize_t res = HANDLE_EINTR(write(file, buf, buf_len));
  if (res == -1) {
    res = RecordAndMapError(errno, FILE_ERROR_SOURCE_WRITE, record_uma,
                            bound_net_log);
  }
  *result = res;
}

// Helper function used for FileStream::Write().
void WriteFileToIOBuffer(base::PlatformFile file,
                         scoped_refptr<IOBuffer> buf,
                         int buf_len,
                         bool record_uma,
                         int* result,
                         base::WaitableEvent* on_complete,
                         const net::BoundNetLog& bound_net_log) {
  WriteFile(file, buf->data(), buf_len, record_uma, result, bound_net_log);
  on_complete->Signal();
}

// FlushFile() is a simple wrapper around fsync() that handles EINTR signals and
// calls MapSystemError() to map errno to net error codes.  It tries to flush to
// completion.
int FlushFile(base::PlatformFile file,
              bool record_uma,
              const net::BoundNetLog& bound_net_log) {
  base::ThreadRestrictions::AssertIOAllowed();
  ssize_t res = HANDLE_EINTR(fsync(file));
  if (res == -1) {
    res = RecordAndMapError(errno, FILE_ERROR_SOURCE_FLUSH, record_uma,
                            bound_net_log);
  }
  return res;
}

}  // namespace

// FileStream ------------------------------------------------------------

FileStream::FileStream(net::NetLog* net_log)
    : file_(base::kInvalidPlatformFileValue),
      open_flags_(0),
      auto_closed_(true),
      record_uma_(false),
      bound_net_log_(net::BoundNetLog::Make(net_log,
                                            net::NetLog::SOURCE_FILESTREAM)),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  bound_net_log_.BeginEvent(net::NetLog::TYPE_FILE_STREAM_ALIVE, NULL);
}

FileStream::FileStream(base::PlatformFile file, int flags, net::NetLog* net_log)
    : file_(file),
      open_flags_(flags),
      auto_closed_(false),
      record_uma_(false),
      bound_net_log_(net::BoundNetLog::Make(net_log,
                                            net::NetLog::SOURCE_FILESTREAM)),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  bound_net_log_.BeginEvent(net::NetLog::TYPE_FILE_STREAM_ALIVE, NULL);
}

FileStream::~FileStream() {
  if (auto_closed_) {
    if (open_flags_ & base::PLATFORM_FILE_ASYNC) {
      // Block until the last read/write operation is complete, if needed.
      // This is needed to close the file safely.
      // TODO(satorux): Remove the blocking logic.
      if (on_io_complete_.get())
        on_io_complete_->Wait();
      const bool posted = base::WorkerPool::PostTask(
          FROM_HERE,
          base::Bind(&CloseFile, file_, bound_net_log_),
          true /* task_is_slow */);
      DCHECK(posted);
    } else {
      CloseSync();
    }
  }

  bound_net_log_.EndEvent(net::NetLog::TYPE_FILE_STREAM_ALIVE, NULL);
}

void FileStream::Close(const CompletionCallback& callback) {
  DCHECK(open_flags_ & base::PLATFORM_FILE_ASYNC);
  DCHECK(callback_.is_null());

  callback_ = callback;
  const bool posted = base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CloseFile, file_, bound_net_log_),
      base::Bind(&FileStream::OnClosed, weak_ptr_factory_.GetWeakPtr()),
      true /* task_is_slow */);
  DCHECK(posted);
}

void FileStream::CloseSync() {
  // Abort any existing asynchronous operations.

  // TODO(satorux): Replace this with a DCHECK once once all async clients
  // are migrated to use Close(). crbug.com/114783
  //
  // DCHECK(!(open_flags_ & base::PLATFORM_FILE_ASYNC));
  weak_ptr_factory_.InvalidateWeakPtrs();

  CloseFile(file_, bound_net_log_);
  file_ = base::kInvalidPlatformFileValue;
}

int FileStream::Open(const FilePath& path, int open_flags,
                     const CompletionCallback& callback) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  DCHECK(callback_.is_null());
  callback_ = callback;

  open_flags_ = open_flags;
  DCHECK(open_flags_ & base::PLATFORM_FILE_ASYNC);

  base::PlatformFile* file =
      new base::PlatformFile(base::kInvalidPlatformFileValue);
  int* result = new int(OK);
  const bool posted = base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&OpenFile, path, open_flags, record_uma_, file, result,
                 bound_net_log_),
      base::Bind(&FileStream::OnOpened,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(file),
                 base::Owned(result)),
      true /* task_is_slow */);
  DCHECK(posted);
  return ERR_IO_PENDING;
}

int FileStream::OpenSync(const FilePath& path, int open_flags) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  open_flags_ = open_flags;
  // TODO(satorux): Put a DCHECK once once all async clients are migrated
  // to use Open(). crbug.com/114783
  //
  // DCHECK(!(open_flags_ & base::PLATFORM_FILE_ASYNC));

  int result = OK;
  OpenFile(path, open_flags_, record_uma_, &file_, &result, bound_net_log_);
  return result;
}

bool FileStream::IsOpen() const {
  return file_ != base::kInvalidPlatformFileValue;
}

int64 FileStream::Seek(Whence whence, int64 offset) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return ERR_UNEXPECTED;

  // If we're in async, make sure we don't have a request in flight.
  DCHECK(!(open_flags_ & base::PLATFORM_FILE_ASYNC) ||
         callback_.is_null());

  off_t res = lseek(file_, static_cast<off_t>(offset),
                    static_cast<int>(whence));
  if (res == static_cast<off_t>(-1)) {
    return RecordAndMapError(errno,
                             FILE_ERROR_SOURCE_SEEK,
                             record_uma_,
                             bound_net_log_);
  }

  return res;
}

int64 FileStream::Available() {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return ERR_UNEXPECTED;

  int64 cur_pos = Seek(FROM_CURRENT, 0);
  if (cur_pos < 0)
    return cur_pos;

  struct stat info;
  if (fstat(file_, &info) != 0) {
    return RecordAndMapError(errno,
                             FILE_ERROR_SOURCE_GET_SIZE,
                             record_uma_,
                             bound_net_log_);
  }

  int64 size = static_cast<int64>(info.st_size);
  DCHECK_GT(size, cur_pos);

  return size - cur_pos;
}

int FileStream::Read(
    IOBuffer* in_buf, int buf_len, const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);
  DCHECK(open_flags_ & base::PLATFORM_FILE_READ);
  DCHECK(open_flags_ & base::PLATFORM_FILE_ASYNC);
  // Make sure we don't have a request in flight.
  DCHECK(callback_.is_null());

  callback_ = callback;
  int* result = new int(OK);
  scoped_refptr<IOBuffer> buf = in_buf;
  DCHECK(!on_io_complete_.get());
  on_io_complete_.reset(new base::WaitableEvent(
      false  /* manual_reset */, false  /* initially_signaled */));
  const bool posted = base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ReadFileFromIOBuffer, file_, buf, buf_len,
                 record_uma_, result, on_io_complete_.get(), bound_net_log_),
      base::Bind(&FileStream::OnIOComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(result)),
      true /* task is slow */);
  DCHECK(posted);
  return ERR_IO_PENDING;
}

int FileStream::ReadSync(char* buf, int buf_len) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(!(open_flags_ & base::PLATFORM_FILE_ASYNC));
  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);
  DCHECK(open_flags_ & base::PLATFORM_FILE_READ);

  int result = OK;
  ReadFile(file_, buf, buf_len, record_uma_, &result, bound_net_log_);
  return result;
}

int FileStream::ReadUntilComplete(char *buf, int buf_len) {
  int to_read = buf_len;
  int bytes_total = 0;

  do {
    int bytes_read = ReadSync(buf, to_read);
    if (bytes_read <= 0) {
      if (bytes_total == 0)
        return bytes_read;

      return bytes_total;
    }

    bytes_total += bytes_read;
    buf += bytes_read;
    to_read -= bytes_read;
  } while (bytes_total < buf_len);

  return bytes_total;
}

int FileStream::Write(
    IOBuffer* in_buf, int buf_len, const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(open_flags_ & base::PLATFORM_FILE_ASYNC);
  // write(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);
  // Make sure we don't have a request in flight.
  DCHECK(callback_.is_null());

  callback_ = callback;
  int* result = new int(OK);
  scoped_refptr<IOBuffer> buf = in_buf;
  DCHECK(!on_io_complete_.get());
  on_io_complete_.reset(new base::WaitableEvent(
      false  /* manual_reset */, false  /* initially_signaled */));
  const bool posted = base::WorkerPool::PostTaskAndReply(
      FROM_HERE,
      base::Bind(&WriteFileToIOBuffer, file_, buf, buf_len,
                 record_uma_, result, on_io_complete_.get(), bound_net_log_),
      base::Bind(&FileStream::OnIOComplete,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(result)),
      true /* task is slow */);
  DCHECK(posted);
  return ERR_IO_PENDING;
}

int FileStream::WriteSync(
    const char* buf, int buf_len) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(!(open_flags_ & base::PLATFORM_FILE_ASYNC));
  // write(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);

  int result = OK;
  WriteFile(file_, buf, buf_len, record_uma_, &result, bound_net_log_);
  return result;
}

int64 FileStream::Truncate(int64 bytes) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return ERR_UNEXPECTED;

  // We'd better be open for writing.
  DCHECK(open_flags_ & base::PLATFORM_FILE_WRITE);

  // Seek to the position to truncate from.
  int64 seek_position = Seek(FROM_BEGIN, bytes);
  if (seek_position != bytes)
    return ERR_UNEXPECTED;

  // And truncate the file.
  int result = ftruncate(file_, bytes);
  if (result == 0)
    return seek_position;

  return RecordAndMapError(errno,
                           FILE_ERROR_SOURCE_SET_EOF,
                           record_uma_,
                           bound_net_log_);
}

int FileStream::Flush() {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  return FlushFile(file_, record_uma_, bound_net_log_);
}

void FileStream::EnableErrorStatistics() {
  record_uma_ = true;
}

void FileStream::SetBoundNetLogSource(
    const net::BoundNetLog& owner_bound_net_log) {
  if ((owner_bound_net_log.source().id == net::NetLog::Source::kInvalidId) &&
      (bound_net_log_.source().id == net::NetLog::Source::kInvalidId)) {
    // Both |BoundNetLog|s are invalid.
    return;
  }

  // Should never connect to itself.
  DCHECK_NE(bound_net_log_.source().id, owner_bound_net_log.source().id);

  bound_net_log_.AddEvent(
      net::NetLog::TYPE_FILE_STREAM_BOUND_TO_OWNER,
      make_scoped_refptr(
          new net::NetLogSourceParameter("source_dependency",
                                         owner_bound_net_log.source())));

  owner_bound_net_log.AddEvent(
      net::NetLog::TYPE_FILE_STREAM_SOURCE,
      make_scoped_refptr(
          new net::NetLogSourceParameter("source_dependency",
                                         bound_net_log_.source())));
}

void FileStream::OnClosed() {
  file_ = base::kInvalidPlatformFileValue;

  CompletionCallback temp = callback_;
  callback_.Reset();
  temp.Run(OK);
}

void FileStream::OnOpened(base::PlatformFile* file, int* result) {
  file_ = *file;

  CompletionCallback temp = callback_;
  callback_.Reset();
  temp.Run(*result);
}

void FileStream::OnIOComplete(int* result) {
  CompletionCallback temp_callback = callback_;
  callback_.Reset();

  // Reset this earlier than Run() as Run() may end up calling Read/Write().
  on_io_complete_.reset();
  temp_callback.Run(*result);
}

}  // namespace net
