// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/encoded_bitstream_buffer.h"

#include "base/logging.h"

namespace media {

EncodedBitstreamBuffer::EncodedBitstreamBuffer(
    int buffer_id,
    uint8* buffer,
    uint32 size,
    base::SharedMemoryHandle handle,
    const media::BufferEncodingMetadata& metadata,
    const base::Closure& destroy_cb)
    : buffer_id_(buffer_id),
      buffer_(buffer),
      size_(size),
      shared_memory_handle_(handle),
      metadata_(metadata),
      destroy_cb_(destroy_cb) {
}

EncodedBitstreamBuffer::~EncodedBitstreamBuffer() {
  destroy_cb_.Run();
}

int EncodedBitstreamBuffer::buffer_id() const {
  return buffer_id_;
}

const uint8* EncodedBitstreamBuffer::buffer() const {
  return buffer_;
}

uint32 EncodedBitstreamBuffer::size() const {
  return size_;
}

base::SharedMemoryHandle EncodedBitstreamBuffer::shared_memory_handle() const {
  return shared_memory_handle_;
}

const media::BufferEncodingMetadata& EncodedBitstreamBuffer::metadata() const {
  return metadata_;
}

}  // namespace media
