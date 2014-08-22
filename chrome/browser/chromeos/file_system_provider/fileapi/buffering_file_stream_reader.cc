// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/buffering_file_stream_reader.h"

#include <algorithm>

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace chromeos {
namespace file_system_provider {

BufferingFileStreamReader::BufferingFileStreamReader(
    scoped_ptr<storage::FileStreamReader> file_stream_reader,
    int buffer_size)
    : file_stream_reader_(file_stream_reader.Pass()),
      buffer_size_(buffer_size),
      preloading_buffer_(new net::IOBuffer(buffer_size_)),
      preloading_buffer_offset_(0),
      buffered_bytes_(0),
      weak_ptr_factory_(this) {
}

BufferingFileStreamReader::~BufferingFileStreamReader() {
}

int BufferingFileStreamReader::Read(net::IOBuffer* buffer,
                                    int buffer_length,
                                    const net::CompletionCallback& callback) {
  // Return as much as available in the internal buffer. It may be less than
  // |buffer_length|, what is valid.
  const int bytes_read =
      CopyFromPreloadingBuffer(make_scoped_refptr(buffer), buffer_length);
  if (bytes_read)
    return bytes_read;

  // If the internal buffer is empty, and more bytes than the internal buffer
  // size is requested, then call the internal file stream reader directly.
  if (buffer_length >= buffer_size_) {
    const int result =
        file_stream_reader_->Read(buffer, buffer_length, callback);
    DCHECK_EQ(result, net::ERR_IO_PENDING);
    return result;
  }

  // Nothing copied, so contents have to be preloaded.
  Preload(base::Bind(&BufferingFileStreamReader::OnPreloadCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     make_scoped_refptr(buffer),
                     buffer_length,
                     callback));

  return net::ERR_IO_PENDING;
}

int64 BufferingFileStreamReader::GetLength(
    const net::Int64CompletionCallback& callback) {
  const int64 result = file_stream_reader_->GetLength(callback);
  DCHECK_EQ(net::ERR_IO_PENDING, result);

  return result;
}

int BufferingFileStreamReader::CopyFromPreloadingBuffer(
    scoped_refptr<net::IOBuffer> buffer,
    int buffer_length) {
  const int read_bytes = std::min(buffer_length, buffered_bytes_);

  memcpy(buffer->data(),
         preloading_buffer_->data() + preloading_buffer_offset_,
         read_bytes);
  preloading_buffer_offset_ += read_bytes;
  buffered_bytes_ -= read_bytes;

  return read_bytes;
}

void BufferingFileStreamReader::Preload(
    const net::CompletionCallback& callback) {
  // TODO(mtomasz): Dynamically calculate the chunk size. Start from a small
  // one, then increase for consecutive requests. That would improve performance
  // when reading just small chunks, instead of the entire file.
  const int preload_bytes = buffer_size_;

  const int result =
      file_stream_reader_->Read(preloading_buffer_, preload_bytes, callback);
  DCHECK_EQ(result, net::ERR_IO_PENDING);
}

void BufferingFileStreamReader::OnPreloadCompleted(
    scoped_refptr<net::IOBuffer> buffer,
    int buffer_length,
    const net::CompletionCallback& callback,
    int result) {
  if (result < 0) {
    callback.Run(result);
    return;
  }

  preloading_buffer_offset_ = 0;
  buffered_bytes_ = result;

  callback.Run(CopyFromPreloadingBuffer(buffer, buffer_length));
}

}  // namespace file_system_provider
}  // namespace chromeos
