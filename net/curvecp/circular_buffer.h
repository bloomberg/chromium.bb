// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_CIRCULAR_BUFFER_H_
#define NET_CURVECP_CIRCULAR_BUFFER_H_
#pragma once

namespace net {

// A circular buffer is a fixed sized buffer which can be read or written
class CircularBuffer {
 public:
  // Create a CircularBuffer with maximum size |capacity|.
  explicit CircularBuffer(int capacity);
  ~CircularBuffer();

  int length() const { return length_; }

  // Writes data into the circular buffer.
  // |data| is the bytes to write.
  // |length| is the number of bytes to write.
  // Returns the number of bytes written, which may be less than |length| or
  // 0 if no space is available in the buffer.
  int write(const char* data, int length);

  // Reads up to |length| bytes from the buffer into |data|.
  // Returns the number of bytes read, or 0 if no data is available.
  int read(char* data, int length);

 private:
  int capacity_;
  int head_;
  int length_;
  char* buffer_;
};

}  // namespace net

#endif  //  NET_CURVECP_CIRCULAR_BUFFER_H_
