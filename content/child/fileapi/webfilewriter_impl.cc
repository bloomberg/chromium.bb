// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/fileapi/webfilewriter_impl.h"

#include "base/bind.h"
#include "content/child/child_thread.h"
#include "content/child/fileapi/file_system_dispatcher.h"

namespace content {

namespace {

inline FileSystemDispatcher* GetFileSystemDispatcher() {
  return ChildThread::current()->file_system_dispatcher();
}

}  // namespace

WebFileWriterImpl::WebFileWriterImpl(
     const GURL& path, WebKit::WebFileWriterClient* client)
  : WebFileWriterBase(path, client),
    request_id_(0) {
}

WebFileWriterImpl::~WebFileWriterImpl() {
}

void WebFileWriterImpl::DoTruncate(const GURL& path, int64 offset) {
  // The FileSystemDispatcher takes ownership of the CallbackDispatcher.
  GetFileSystemDispatcher()->Truncate(
      path, offset, &request_id_,
      base::Bind(&WebFileWriterImpl::DidFinish, AsWeakPtr()));
}

void WebFileWriterImpl::DoWrite(
    const GURL& path, const GURL& blob_url, int64 offset) {
  GetFileSystemDispatcher()->Write(
      path, blob_url, offset, &request_id_,
      base::Bind(&WebFileWriterImpl::DidWrite, AsWeakPtr()),
      base::Bind(&WebFileWriterImpl::DidFinish, AsWeakPtr()));
}

void WebFileWriterImpl::DoCancel() {
  GetFileSystemDispatcher()->Cancel(
      request_id_,
      base::Bind(&WebFileWriterImpl::DidFinish, AsWeakPtr()));
}

}  // namespace content
