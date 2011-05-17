// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/curvecp/circular_buffer.h"

#include <algorithm>
#include "base/logging.h"

namespace net {

CircularBuffer::CircularBuffer(int capacity)
    : capacity_(capacity),
      head_(0),
      length_(0) {
  buffer_ = new char[capacity_];
}

CircularBuffer::~CircularBuffer() {
  delete [] buffer_;
}

int CircularBuffer::write(const char* data, int length) {
  int bytes_written = 0;

  // We can't write more bytes than we have space for.
  if (length > capacity_ - length_)
    length = capacity_ - length_;

  // Check for space at end of buffer.
  if (head_ + length_ < capacity_) {
    bytes_written = std::min(capacity_ - head_ - length_, length);
    int end_pos = head_ + length_;

    memcpy(buffer_ + end_pos, data, bytes_written);
    length_ += bytes_written;
    length -= bytes_written;
  }

  if (!length)
    return bytes_written;

  DCHECK_LT(length_, capacity_);  // Buffer still has space.
  DCHECK_GE(head_ + length_, capacity_);  // Space is at the beginning.

  int start_pos = (head_ + length_) % capacity_;
  memcpy(&buffer_[start_pos], &data[bytes_written], length);
  bytes_written += length;
  length_ += length;
  return bytes_written;
}

int CircularBuffer::read(char* data, int length) {
  int bytes_read = 0;

  // We can't read more bytes than are in the buffer.
  if (length > length_)
    length = length_;

  while (length) {
    DCHECK_LE(length, length_);
    int span_end = std::min(capacity_, head_ + length);
    int len = span_end - head_;
    DCHECK_LE(len, length_);
    memcpy(&data[bytes_read], &buffer_[head_], len);
    bytes_read += len;
    length -= len;
    length_ -= len;
    head_ += len;
    DCHECK_LE(head_, capacity_);
    if (head_ == capacity_)
      head_ = 0;
  }
  return bytes_read;
}

}  // namespace net
