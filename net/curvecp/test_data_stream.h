// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_TEST_DATA_STREAM_H_
#define NET_CURVECP_TEST_DATA_STREAM_H_
#pragma once

#include <string.h>  // for memcpy()
#include <algorithm>

// This is a test class for generating an infinite stream of data which can
// be verified independently to be the correct stream of data.

namespace net {

class TestDataStream {
 public:
  TestDataStream() {
    Reset();
  }

  // Fill |buffer| with |length| bytes of data from the stream.
  void GetBytes(char* buffer, int length) {
    while (length) {
      AdvanceIndex();
      int bytes_to_copy = std::min(length, bytes_remaining_);
      memcpy(buffer, buffer_ptr_, bytes_to_copy);
      buffer += bytes_to_copy;
      Consume(bytes_to_copy);
      length -= bytes_to_copy;
    }
  }

  // Verify that |buffer| contains the expected next |length| bytes from the
  // stream.  Returns true if correct, false otherwise.
  bool VerifyBytes(const char *buffer, int length) {
    while (length) {
      AdvanceIndex();
      int bytes_to_compare = std::min(length, bytes_remaining_);
      if (memcmp(buffer, buffer_ptr_, bytes_to_compare))
        return false;
      Consume(bytes_to_compare);
      length -= bytes_to_compare;
      buffer += bytes_to_compare;
    }
    return true;
  }

  void Reset() {
    index_ = 0;
    bytes_remaining_ = 0;
    buffer_ptr_ = buffer_;
  }

 private:
  // If there is no data spilled over from the previous index, advance the
  // index and fill the buffer.
  void AdvanceIndex() {
    if (bytes_remaining_ == 0) {
      // Convert it to ascii, but don't bother to reverse it.
      // (e.g. 12345 becomes "54321")
      int val = index_++;
      do {
        buffer_[bytes_remaining_++] = (val % 10) + '0';
      } while ((val /= 10) > 0);
      buffer_[bytes_remaining_++] = '.';
    }
  }

  // Consume data from the spill buffer.
  void Consume(int bytes) {
    bytes_remaining_ -= bytes;
    if (bytes_remaining_)
      buffer_ptr_ += bytes;
    else
      buffer_ptr_ = buffer_;
  }

  int index_;
  int bytes_remaining_;
  char buffer_[16];
  char* buffer_ptr_;
};

}  // namespace net

#endif  // NET_CURVECP_TEST_DATA_STREAM_H_
