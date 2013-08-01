// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/fileapi/webfilewriter_impl.h"

#include "base/bind.h"
#include "base/platform_file.h"
#include "content/child/child_thread.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "webkit/child/worker_task_runner.h"

using webkit_glue::WorkerTaskRunner;

namespace content {

namespace {

FileSystemDispatcher* GetFileSystemDispatcher() {
  return ChildThread::current() ?
      ChildThread::current()->file_system_dispatcher() : NULL;
}

}  // namespace

typedef FileSystemDispatcher::StatusCallback StatusCallback;
typedef FileSystemDispatcher::WriteCallback WriteCallback;

// This instance may be created outside main thread but runs mainly
// on main thread.
class WebFileWriterImpl::WriterBridge
    : public base::RefCountedThreadSafe<WriterBridge> {
 public:
  WriterBridge()
      : request_id_(0),
        thread_id_(WorkerTaskRunner::Instance()->CurrentWorkerId()) {}

  void Truncate(const GURL& path, int64 offset,
                const StatusCallback& status_callback) {
    status_callback_ = status_callback;
    if (!GetFileSystemDispatcher())
      return;
    ChildThread::current()->file_system_dispatcher()->Truncate(
        path, offset, &request_id_,
        base::Bind(&WriterBridge::DidFinish, this));
  }

  void Write(const GURL& path, const GURL& blob_url, int64 offset,
             const WriteCallback& write_callback,
             const StatusCallback& error_callback) {
    write_callback_ = write_callback;
    status_callback_ = error_callback;
    if (!GetFileSystemDispatcher())
      return;
    ChildThread::current()->file_system_dispatcher()->Write(
        path, blob_url, offset, &request_id_,
        base::Bind(&WriterBridge::DidWrite, this),
        base::Bind(&WriterBridge::DidFinish, this));
  }

  void Cancel(const StatusCallback& status_callback) {
    status_callback_ = status_callback;
    if (!GetFileSystemDispatcher())
      return;
    ChildThread::current()->file_system_dispatcher()->Cancel(
        request_id_,
        base::Bind(&WriterBridge::DidFinish, this));
  }

 private:
  friend class base::RefCountedThreadSafe<WriterBridge>;
  virtual ~WriterBridge() {}

  void DidWrite(int64 bytes, bool complete) {
    PostTaskToWorker(base::Bind(write_callback_, bytes, complete));
  }

  void DidFinish(base::PlatformFileError status) {
    PostTaskToWorker(base::Bind(status_callback_, status));
  }

  void PostTaskToWorker(const base::Closure& closure) {
    if (!thread_id_)
      closure.Run();
    else
      WorkerTaskRunner::Instance()->PostTask(thread_id_, closure);
  }

  StatusCallback status_callback_;
  WriteCallback write_callback_;
  int request_id_;
  int thread_id_;
};

WebFileWriterImpl::WebFileWriterImpl(
     const GURL& path, WebKit::WebFileWriterClient* client,
     base::MessageLoopProxy* main_thread_loop)
  : WebFileWriterBase(path, client),
    main_thread_loop_(main_thread_loop),
    bridge_(new WriterBridge) {
}

WebFileWriterImpl::~WebFileWriterImpl() {
}

void WebFileWriterImpl::DoTruncate(const GURL& path, int64 offset) {
  RunOnMainThread(base::Bind(&WriterBridge::Truncate, bridge_,
      path, offset,
      base::Bind(&WebFileWriterImpl::DidFinish, AsWeakPtr())));
}

void WebFileWriterImpl::DoWrite(
    const GURL& path, const GURL& blob_url, int64 offset) {
  RunOnMainThread(base::Bind(&WriterBridge::Write, bridge_,
      path, blob_url, offset,
      base::Bind(&WebFileWriterImpl::DidWrite, AsWeakPtr()),
      base::Bind(&WebFileWriterImpl::DidFinish, AsWeakPtr())));
}

void WebFileWriterImpl::DoCancel() {
  RunOnMainThread(base::Bind(&WriterBridge::Cancel, bridge_,
      base::Bind(&WebFileWriterImpl::DidFinish, AsWeakPtr())));
}

void WebFileWriterImpl::RunOnMainThread(const base::Closure& closure) {
  if (main_thread_loop_->RunsTasksOnCurrentThread())
    closure.Run();
  else
    main_thread_loop_->PostTask(FROM_HERE, closure);
}

}  // namespace content
