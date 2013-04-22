// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"

#include <algorithm>
#include <cstring>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace drive {
namespace internal {
namespace {

// Copies the content in |pending_data| into |buffer| at most
// |buffer_length| bytes, and erases the copied data from
// |pending_data|. Returns the number of copied bytes.
int ReadInternal(ScopedVector<std::string>* pending_data,
                 net::IOBuffer* buffer, int buffer_length) {
  size_t index = 0;
  int offset = 0;
  for (; index < pending_data->size() && offset < buffer_length; ++index) {
    const std::string& chunk = *(*pending_data)[index];
    DCHECK(!chunk.empty());

    size_t bytes_to_read = std::min(
        chunk.size(), static_cast<size_t>(buffer_length - offset));
    std::memmove(buffer->data() + offset, chunk.data(), bytes_to_read);
    offset += bytes_to_read;
    if (bytes_to_read < chunk.size()) {
      // The chunk still has some remaining data.
      // So remove leading (copied) bytes, and quit the loop so that
      // the remaining data won't be deleted in the following erase().
      (*pending_data)[index]->erase(0, bytes_to_read);
      break;
    }
  }

  // Consume the copied data.
  pending_data->erase(pending_data->begin(), pending_data->begin() + index);

  return offset;
}

}  // namespace

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

NetworkReaderProxy::NetworkReaderProxy(int64 content_length)
    : remaining_content_length_(content_length),
      error_code_(net::OK),
      buffer_length_(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

NetworkReaderProxy::~NetworkReaderProxy() {
}

int NetworkReaderProxy::Read(net::IOBuffer* buffer, int buffer_length,
                             const net::CompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Check if there is no pending Read operation.
  DCHECK(!buffer_);
  DCHECK_EQ(buffer_length_, 0);
  DCHECK(callback_.is_null());
  // Validate the arguments.
  DCHECK(buffer);
  DCHECK_GT(buffer_length, 0);
  DCHECK(!callback.is_null());

  if (error_code_ != net::OK) {
    // An error is already found. Return it immediately.
    return error_code_;
  }

  if (remaining_content_length_ == 0) {
    // If no more data, return immediately.
    return 0;
  }

  if (pending_data_.empty()) {
    // No data is available. Keep the arguments, and return pending status.
    buffer_ = buffer;
    buffer_length_ = buffer_length;
    callback_ = callback;
    return net::ERR_IO_PENDING;
  }

  int result = ReadInternal(&pending_data_, buffer, buffer_length);
  remaining_content_length_ -= result;
  DCHECK_GE(remaining_content_length_, 0);
  return result;
}

void NetworkReaderProxy::OnGetContent(scoped_ptr<std::string> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(data && !data->empty());

  pending_data_.push_back(data.release());
  if (!buffer_) {
    // No pending Read operation.
    return;
  }

  int result = ReadInternal(&pending_data_, buffer_.get(), buffer_length_);
  remaining_content_length_ -= result;
  DCHECK_GE(remaining_content_length_, 0);

  buffer_ = NULL;
  buffer_length_ = 0;
  DCHECK(!callback_.is_null());
  base::ResetAndReturn(&callback_).Run(result);
}

void NetworkReaderProxy::OnError(DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_NE(error, DRIVE_FILE_OK);

  error_code_ =
      net::PlatformFileErrorToNetError(DriveFileErrorToPlatformError(error));
  pending_data_.clear();

  if (callback_.is_null()) {
    // No pending Read operation.
    return;
  }

  buffer_ = NULL;
  buffer_length_ = 0;
  base::ResetAndReturn(&callback_).Run(error_code_);
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
