// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"

using content::BrowserThread;

namespace drive {
namespace internal {

LocalReaderProxy::LocalReaderProxy(scoped_ptr<net::FileStream> file_stream)
    : file_stream_(file_stream.Pass()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(file_stream_);
}

LocalReaderProxy::~LocalReaderProxy() {
}

int LocalReaderProxy::Read(net::IOBuffer* buffer, int buffer_length,
                           const net::CompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(file_stream_);
  return file_stream_->Read(buffer, buffer_length, callback);
}

void LocalReaderProxy::OnGetContent(scoped_ptr<std::string> data) {
  // This method should never be called, because no data should be received
  // from the network during the reading of local-cache file.
  NOTREACHED();
}

void LocalReaderProxy::OnError(DriveFileError error) {
  // This method should never be called, because we don't access to the server
  // during the reading of local-cache file.
  NOTREACHED();
}

}  // namespace internal


DriveFileStreamReader::DriveFileStreamReader() {
}

DriveFileStreamReader::~DriveFileStreamReader() {
}

int DriveFileStreamReader::Read(net::IOBuffer* buf, int buf_len,
                                const net::CompletionCallback& callback) {
  // TODO(hidehiko): Implement this.
  NOTIMPLEMENTED();
  return 0;
}

int64 DriveFileStreamReader::GetLength(
    const net::Int64CompletionCallback& callback) {
  // TODO(hidehiko): Implement this.
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace drive
