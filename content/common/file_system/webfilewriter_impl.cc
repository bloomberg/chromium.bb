// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/file_system/webfilewriter_impl.h"

#include "content/common/child_thread.h"
#include "content/common/file_system/file_system_dispatcher.h"

namespace {

inline FileSystemDispatcher* GetFileSystemDispatcher() {
  return ChildThread::current()->file_system_dispatcher();
}
}

class WebFileWriterImpl::CallbackDispatcher
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  explicit CallbackDispatcher(
      const base::WeakPtr<WebFileWriterImpl>& writer) : writer_(writer) {
  }
  virtual ~CallbackDispatcher() {
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo&) {
    NOTREACHED();
  }
  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) {
    NOTREACHED();
  }
  virtual void DidOpenFileSystem(const std::string& name,
                                 const FilePath& root_path) {
    NOTREACHED();
  }
  virtual void DidSucceed() {
    if (writer_)
      writer_->DidSucceed();
  }
  virtual void DidFail(base::PlatformFileError error_code) {
    if (writer_)
      writer_->DidFail(error_code);
  }
  virtual void DidWrite(int64 bytes, bool complete) {
    if (writer_)
      writer_->DidWrite(bytes, complete);
  }

 private:
  base::WeakPtr<WebFileWriterImpl> writer_;
};

WebFileWriterImpl::WebFileWriterImpl(
     const WebKit::WebString& path, WebKit::WebFileWriterClient* client)
  : WebFileWriterBase(path, client),
    request_id_(0) {
}

WebFileWriterImpl::~WebFileWriterImpl() {
}

void WebFileWriterImpl::DoTruncate(const FilePath& path, int64 offset) {
  // The FileSystemDispatcher takes ownership of the CallbackDispatcher.
  GetFileSystemDispatcher()->Truncate(path, offset, &request_id_,
                                      new CallbackDispatcher(AsWeakPtr()));
}

void WebFileWriterImpl::DoWrite(
    const FilePath& path, const GURL& blob_url, int64 offset) {
  GetFileSystemDispatcher()->Write(path, blob_url, offset, &request_id_,
                                   new CallbackDispatcher(AsWeakPtr()));
}

void WebFileWriterImpl::DoCancel() {
  GetFileSystemDispatcher()->Cancel(request_id_,
                                    new CallbackDispatcher(AsWeakPtr()));
}
