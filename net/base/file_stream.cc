// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream.h"

#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/worker_pool.h"
#include "net/base/file_stream_context.h"
#include "net/base/net_errors.h"

namespace net {

FileStream::FileStream(NetLog* net_log,
                       const scoped_refptr<base::TaskRunner>& task_runner)
    : bound_net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_FILESTREAM)),
      context_(new Context(bound_net_log_, task_runner)) {
  bound_net_log_.BeginEvent(NetLog::TYPE_FILE_STREAM_ALIVE);
}

FileStream::FileStream(NetLog* net_log)
    : bound_net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_FILESTREAM)),
      context_(new Context(bound_net_log_,
                           base::WorkerPool::GetTaskRunner(true /* slow */))) {
  bound_net_log_.BeginEvent(NetLog::TYPE_FILE_STREAM_ALIVE);
}

FileStream::FileStream(base::PlatformFile file,
                       int flags,
                       NetLog* net_log,
                       const scoped_refptr<base::TaskRunner>& task_runner)
    : bound_net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_FILESTREAM)),
      context_(new Context(base::File(file), bound_net_log_, task_runner)) {
  bound_net_log_.BeginEvent(NetLog::TYPE_FILE_STREAM_ALIVE);
}

FileStream::FileStream(base::PlatformFile file, int flags, NetLog* net_log)
    : bound_net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_FILESTREAM)),
      context_(new Context(base::File(file), bound_net_log_,
                           base::WorkerPool::GetTaskRunner(true /* slow */))) {
  bound_net_log_.BeginEvent(NetLog::TYPE_FILE_STREAM_ALIVE);
}

FileStream::FileStream(base::File file,
                       net::NetLog* net_log,
                       const scoped_refptr<base::TaskRunner>& task_runner)
    : bound_net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_FILESTREAM)),
      context_(new Context(file.Pass(), bound_net_log_, task_runner)) {
  bound_net_log_.BeginEvent(NetLog::TYPE_FILE_STREAM_ALIVE);
}

FileStream::FileStream(base::File file, net::NetLog* net_log)
    : bound_net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_FILESTREAM)),
      context_(new Context(file.Pass(), bound_net_log_,
                           base::WorkerPool::GetTaskRunner(true /* slow */))) {
  bound_net_log_.BeginEvent(NetLog::TYPE_FILE_STREAM_ALIVE);
}

FileStream::~FileStream() {
  context_.release()->Orphan();
  bound_net_log_.EndEvent(NetLog::TYPE_FILE_STREAM_ALIVE);
}

int FileStream::Open(const base::FilePath& path, int open_flags,
                     const CompletionCallback& callback) {
  if (IsOpen()) {
    DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  DCHECK(open_flags & base::File::FLAG_ASYNC);
  context_->OpenAsync(path, open_flags, callback);
  return ERR_IO_PENDING;
}

int FileStream::Close(const CompletionCallback& callback) {
  context_->CloseAsync(callback);
  return ERR_IO_PENDING;
}

bool FileStream::IsOpen() const {
  return context_->file().IsValid();
}

int FileStream::Seek(Whence whence,
                     int64 offset,
                     const Int64CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->SeekAsync(whence, offset, callback);
  return ERR_IO_PENDING;
}

int FileStream::Read(IOBuffer* buf,
                     int buf_len,
                     const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  // read(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);

  return context_->ReadAsync(buf, buf_len, callback);
}

int FileStream::Write(IOBuffer* buf,
                      int buf_len,
                      const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  // write(..., 0) will return 0, which indicates end-of-file.
  DCHECK_GT(buf_len, 0);

  return context_->WriteAsync(buf, buf_len, callback);
}

int FileStream::Flush(const CompletionCallback& callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->FlushAsync(callback);
  return ERR_IO_PENDING;
}

const base::File& FileStream::GetFileForTesting() const {
  return context_->file();
}

}  // namespace net
