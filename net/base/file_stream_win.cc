// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream.h"

#include <windows.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/file_stream_metrics.h"
#include "net/base/file_stream_net_log_parameters.h"
#include "net/base/net_errors.h"

namespace net {

// Ensure that we can just use our Whence values directly.
COMPILE_ASSERT(FROM_BEGIN == FILE_BEGIN, bad_whence_begin);
COMPILE_ASSERT(FROM_CURRENT == FILE_CURRENT, bad_whence_current);
COMPILE_ASSERT(FROM_END == FILE_END, bad_whence_end);

static void SetOffset(OVERLAPPED* overlapped, const LARGE_INTEGER& offset) {
  overlapped->Offset = offset.LowPart;
  overlapped->OffsetHigh = offset.HighPart;
}

static void IncrementOffset(OVERLAPPED* overlapped, DWORD count) {
  LARGE_INTEGER offset;
  offset.LowPart = overlapped->Offset;
  offset.HighPart = overlapped->OffsetHigh;
  offset.QuadPart += static_cast<LONGLONG>(count);
  SetOffset(overlapped, offset);
}

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

}  // namespace

// FileStream::AsyncContext ----------------------------------------------

class FileStream::AsyncContext : public MessageLoopForIO::IOHandler {
 public:
  explicit AsyncContext(const net::BoundNetLog& bound_net_log)
      : context_(), is_closing_(false),
        record_uma_(false), bound_net_log_(bound_net_log),
        error_source_(FILE_ERROR_SOURCE_COUNT) {
    context_.handler = this;
  }
  ~AsyncContext();

  void IOCompletionIsPending(const CompletionCallback& callback);

  OVERLAPPED* overlapped() { return &context_.overlapped; }
  const CompletionCallback& callback() const { return callback_; }

  void set_error_source(FileErrorSource source) { error_source_ = source; }

  void EnableErrorStatistics() {
    record_uma_ = true;
  }

 private:
  virtual void OnIOCompleted(MessageLoopForIO::IOContext* context,
                             DWORD bytes_read, DWORD error) OVERRIDE;

  MessageLoopForIO::IOContext context_;
  CompletionCallback callback_;
  bool is_closing_;
  bool record_uma_;
  const net::BoundNetLog bound_net_log_;
  FileErrorSource error_source_;
};

FileStream::AsyncContext::~AsyncContext() {
  is_closing_ = true;
  bool waited = false;
  base::TimeTicks start = base::TimeTicks::Now();
  while (!callback_.is_null()) {
    waited = true;
    MessageLoopForIO::current()->WaitForIOCompletion(INFINITE, this);
  }
  if (waited) {
    // We want to see if we block the message loop for too long.
    UMA_HISTOGRAM_TIMES("AsyncIO.FileStreamClose",
                        base::TimeTicks::Now() - start);
  }
}

void FileStream::AsyncContext::IOCompletionIsPending(
    const CompletionCallback& callback) {
  DCHECK(callback_.is_null());
  callback_ = callback;
}

void FileStream::AsyncContext::OnIOCompleted(
    MessageLoopForIO::IOContext* context, DWORD bytes_read, DWORD error) {
  DCHECK_EQ(&context_, context);
  DCHECK(!callback_.is_null());

  if (is_closing_) {
    callback_.Reset();
    return;
  }

  int result = static_cast<int>(bytes_read);
  if (error && error != ERROR_HANDLE_EOF) {
    result = RecordAndMapError(error, error_source_, record_uma_,
                               bound_net_log_);
  }

  if (bytes_read)
    IncrementOffset(&context->overlapped, bytes_read);

  CompletionCallback temp;
  std::swap(temp, callback_);
  temp.Run(result);
}

// FileStream ------------------------------------------------------------

FileStream::FileStream(net::NetLog* net_log)
    : file_(base::kInvalidPlatformFileValue),
      open_flags_(0),
      auto_closed_(true),
      record_uma_(false),
      bound_net_log_(net::BoundNetLog::Make(net_log,
                                            net::NetLog::SOURCE_FILESTREAM)) {
  bound_net_log_.BeginEvent(net::NetLog::TYPE_FILE_STREAM_ALIVE, NULL);
}

FileStream::FileStream(base::PlatformFile file, int flags, net::NetLog* net_log)
    : file_(file),
      open_flags_(flags),
      auto_closed_(false),
      record_uma_(false),
      bound_net_log_(net::BoundNetLog::Make(net_log,
                                            net::NetLog::SOURCE_FILESTREAM)) {
  bound_net_log_.BeginEvent(net::NetLog::TYPE_FILE_STREAM_ALIVE, NULL);

  // If the file handle is opened with base::PLATFORM_FILE_ASYNC, we need to
  // make sure we will perform asynchronous File IO to it.
  if (flags & base::PLATFORM_FILE_ASYNC) {
    async_context_.reset(new AsyncContext(bound_net_log_));
    MessageLoopForIO::current()->RegisterIOHandler(file_,
                                                   async_context_.get());
  }
}

FileStream::~FileStream() {
  if (auto_closed_)
    CloseSync();

  bound_net_log_.EndEvent(net::NetLog::TYPE_FILE_STREAM_ALIVE, NULL);
}

void FileStream::CloseSync() {
  bound_net_log_.AddEvent(net::NetLog::TYPE_FILE_STREAM_CLOSE, NULL);
  if (file_ != INVALID_HANDLE_VALUE)
    CancelIo(file_);

  async_context_.reset();
  if (file_ != INVALID_HANDLE_VALUE) {
    if (!base::ClosePlatformFile(file_))
      NOTREACHED();
    file_ = INVALID_HANDLE_VALUE;

    bound_net_log_.EndEvent(net::NetLog::TYPE_FILE_STREAM_OPEN, NULL);
  }
}

int FileStream::OpenSync(const FilePath& path, int open_flags) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  bound_net_log_.BeginEvent(
      net::NetLog::TYPE_FILE_STREAM_OPEN,
      make_scoped_refptr(
          new net::NetLogStringParameter("file_name",
                                         path.AsUTF8Unsafe())));

  open_flags_ = open_flags;
  file_ = base::CreatePlatformFile(path, open_flags_, NULL, NULL);
  if (file_ == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    LOG(WARNING) << "Failed to open file: " << error;
    int net_error = RecordAndMapError(error,
                                      FILE_ERROR_SOURCE_OPEN,
                                      record_uma_,
                                      bound_net_log_);
    bound_net_log_.EndEvent(net::NetLog::TYPE_FILE_STREAM_OPEN, NULL);
    return net_error;
  }

  if (open_flags_ & base::PLATFORM_FILE_ASYNC) {
    async_context_.reset(new AsyncContext(bound_net_log_));
    if (record_uma_)
      async_context_->EnableErrorStatistics();
    MessageLoopForIO::current()->RegisterIOHandler(file_,
                                                   async_context_.get());
  }

  return OK;
}

bool FileStream::IsOpen() const {
  return file_ != INVALID_HANDLE_VALUE;
}

int64 FileStream::Seek(Whence whence, int64 offset) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(!async_context_.get() || async_context_->callback().is_null());

  LARGE_INTEGER distance, result;
  distance.QuadPart = offset;
  DWORD move_method = static_cast<DWORD>(whence);
  if (!SetFilePointerEx(file_, distance, &result, move_method)) {
    DWORD error = GetLastError();
    LOG(WARNING) << "SetFilePointerEx failed: " << error;
    return RecordAndMapError(error,
                             FILE_ERROR_SOURCE_SEEK,
                             record_uma_,
                             bound_net_log_);
  }
  if (async_context_.get()) {
    async_context_->set_error_source(FILE_ERROR_SOURCE_SEEK);
    SetOffset(async_context_->overlapped(), result);
  }
  return result.QuadPart;
}

int64 FileStream::Available() {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return ERR_UNEXPECTED;

  int64 cur_pos = Seek(FROM_CURRENT, 0);
  if (cur_pos < 0)
    return cur_pos;

  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(file_, &file_size)) {
    DWORD error = GetLastError();
    LOG(WARNING) << "GetFileSizeEx failed: " << error;
    return RecordAndMapError(error,
                             FILE_ERROR_SOURCE_GET_SIZE,
                             record_uma_,
                             bound_net_log_);
  }

  return file_size.QuadPart - cur_pos;
}

int FileStream::Read(
    char* buf, int buf_len, const CompletionCallback& callback) {
  DCHECK(async_context_.get());
  return ReadInternal(buf, buf_len, callback);
}

int FileStream::ReadSync(char* buf, int buf_len) {
  DCHECK(!async_context_.get());
  return ReadInternal(buf, buf_len, CompletionCallback());
}

int FileStream::ReadInternal(
    char* buf, int buf_len, const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(open_flags_ & base::PLATFORM_FILE_READ);

  OVERLAPPED* overlapped = NULL;
  if (async_context_.get()) {
    DCHECK(!callback.is_null());
    DCHECK(async_context_->callback().is_null());
    overlapped = async_context_->overlapped();
    async_context_->set_error_source(FILE_ERROR_SOURCE_READ);
  } else {
    DCHECK(callback.is_null());
    base::ThreadRestrictions::AssertIOAllowed();
  }

  int rv;

  DWORD bytes_read;
  if (!ReadFile(file_, buf, buf_len, &bytes_read, overlapped)) {
    DWORD error = GetLastError();
    if (async_context_.get() && error == ERROR_IO_PENDING) {
      async_context_->IOCompletionIsPending(callback);
      rv = ERR_IO_PENDING;
    } else if (error == ERROR_HANDLE_EOF) {
      rv = 0;  // Report EOF by returning 0 bytes read.
    } else {
      LOG(WARNING) << "ReadFile failed: " << error;
      rv = RecordAndMapError(error,
                             FILE_ERROR_SOURCE_READ,
                             record_uma_,
                             bound_net_log_);
    }
  } else if (overlapped) {
    async_context_->IOCompletionIsPending(callback);
    rv = ERR_IO_PENDING;
  } else {
    rv = static_cast<int>(bytes_read);
  }
  return rv;
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
    const char* buf, int buf_len, const CompletionCallback& callback) {
  DCHECK(async_context_.get());
  return WriteInternal(buf, buf_len, callback);
}

int FileStream::WriteSync(
    const char* buf, int buf_len) {
  DCHECK(!async_context_.get());
  return WriteInternal(buf, buf_len, CompletionCallback());
}

int FileStream::WriteInternal(
    const char* buf, int buf_len, const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(open_flags_ & base::PLATFORM_FILE_WRITE);

  OVERLAPPED* overlapped = NULL;
  if (async_context_.get()) {
    DCHECK(!callback.is_null());
    DCHECK(async_context_->callback().is_null());
    overlapped = async_context_->overlapped();
    async_context_->set_error_source(FILE_ERROR_SOURCE_WRITE);
  } else {
    DCHECK(callback.is_null());
    base::ThreadRestrictions::AssertIOAllowed();
  }

  int rv;
  DWORD bytes_written;
  if (!WriteFile(file_, buf, buf_len, &bytes_written, overlapped)) {
    DWORD error = GetLastError();
    if (async_context_.get() && error == ERROR_IO_PENDING) {
      async_context_->IOCompletionIsPending(callback);
      rv = ERR_IO_PENDING;
    } else {
      LOG(WARNING) << "WriteFile failed: " << error;
      rv = RecordAndMapError(error,
                             FILE_ERROR_SOURCE_WRITE,
                             record_uma_,
                             bound_net_log_);
    }
  } else if (overlapped) {
    async_context_->IOCompletionIsPending(callback);
    rv = ERR_IO_PENDING;
  } else {
    rv = static_cast<int>(bytes_written);
  }
  return rv;
}

int FileStream::Flush() {
  base::ThreadRestrictions::AssertIOAllowed();

  if (!IsOpen())
    return ERR_UNEXPECTED;

  DCHECK(open_flags_ & base::PLATFORM_FILE_WRITE);
  if (FlushFileBuffers(file_)) {
    return OK;
  }

  return RecordAndMapError(GetLastError(),
                           FILE_ERROR_SOURCE_FLUSH,
                           record_uma_,
                           bound_net_log_);
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
  BOOL result = SetEndOfFile(file_);
  if (!result) {
    DWORD error = GetLastError();
    LOG(WARNING) << "SetEndOfFile failed: " << error;
    return RecordAndMapError(error,
                             FILE_ERROR_SOURCE_SET_EOF,
                             record_uma_,
                             bound_net_log_);
  }

  // Success.
  return seek_position;
}

void FileStream::EnableErrorStatistics() {
  record_uma_ = true;

  if (async_context_.get())
    async_context_->EnableErrorStatistics();
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

}  // namespace net
