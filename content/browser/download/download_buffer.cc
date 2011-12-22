// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_buffer.h"

#include "net/base/io_buffer.h"

namespace content {

net::IOBuffer* AssembleData(const ContentVector& contents, size_t* num_bytes) {
  if (num_bytes)
    *num_bytes = 0;

  size_t data_len;
  // Determine how large a buffer we need.
  size_t assembly_buffer_length = 0;
  for (size_t i = 0; i < contents.size(); ++i) {
    data_len = contents[i].second;
    assembly_buffer_length += data_len;
  }

  // 0-length IOBuffers are not allowed.
  if (assembly_buffer_length == 0)
    return NULL;

  net::IOBuffer* assembly_buffer = new net::IOBuffer(assembly_buffer_length);

  // Copy the data into |assembly_buffer|.
  size_t bytes_copied = 0;
  for (size_t i = 0; i < contents.size(); ++i) {
    net::IOBuffer* data = contents[i].first;
    data_len = contents[i].second;
    DCHECK(data != NULL);
    DCHECK_LE(bytes_copied + data_len, assembly_buffer_length);
    memcpy(assembly_buffer->data() + bytes_copied, data->data(), data_len);
    bytes_copied += data_len;
  }
  DCHECK_EQ(assembly_buffer_length, bytes_copied);

  if (num_bytes)
    *num_bytes = assembly_buffer_length;

  return assembly_buffer;
}

DownloadBuffer::DownloadBuffer() {
}

DownloadBuffer::~DownloadBuffer() {
}

size_t DownloadBuffer::AddData(net::IOBuffer* io_buffer, size_t byte_count) {
  base::AutoLock auto_lock(lock_);
  contents_.push_back(std::make_pair(io_buffer, byte_count));
  return contents_.size();
}

ContentVector* DownloadBuffer::ReleaseContents() {
  base::AutoLock auto_lock(lock_);
  ContentVector* other_contents = new ContentVector;
  other_contents->swap(contents_);
  return other_contents;
}

size_t DownloadBuffer::size() const {
  base::AutoLock auto_lock(lock_);
  return contents_.size();
}

}  // namespace content
