// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_io_buffer.h"
#include "net/spdy/spdy_stream.h"

namespace net {

SpdyIOBuffer::SpdyIOBuffer() {}

SpdyIOBuffer::SpdyIOBuffer(IOBuffer* buffer, int size, SpdyStream* stream)
  : buffer_(new DrainableIOBuffer(buffer, size)), stream_(stream) {}

SpdyIOBuffer::SpdyIOBuffer(const SpdyIOBuffer& rhs) {
  buffer_ = rhs.buffer_;
  stream_ = rhs.stream_;
}

SpdyIOBuffer::~SpdyIOBuffer() {}

SpdyIOBuffer& SpdyIOBuffer::operator=(const SpdyIOBuffer& rhs) {
  buffer_ = rhs.buffer_;
  stream_ = rhs.stream_;
  return *this;
}

void SpdyIOBuffer::Swap(SpdyIOBuffer* other) {
  buffer_.swap(other->buffer_);
  stream_.swap(other->stream_);
}

void SpdyIOBuffer::Release() {
  buffer_ = NULL;
  stream_ = NULL;
}

}  // namespace net
