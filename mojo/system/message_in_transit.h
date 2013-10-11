// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_
#define MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_

#include <stdint.h>
#include <stdlib.h>  // For |free()|.

#include "base/basictypes.h"
#include "mojo/public/system/system_export.h"

namespace mojo {
namespace system {

// This class is used to represent data in transit. It is thread-unsafe.
// Note: This class is POD.
class MOJO_SYSTEM_EXPORT MessageInTransit {
 public:
  // Creates a |MessageInTransit| with the data given by |bytes|/|num_bytes|.
  static MessageInTransit* Create(const void* bytes, uint32_t num_bytes);

  // Destroys a |MessageInTransit| created using |Create()|.
  inline void Destroy() {
    // No need to call the destructor, since we're POD.
    free(this);
  }

  // Gets the size of the data (in number of bytes).
  uint32_t data_size() const {
    return size_;
  }

  // Gets the data (of size |size()| bytes).
  const void* data() const {
    return reinterpret_cast<const char*>(this) + sizeof(*this);
  }

  size_t size_with_header_and_padding() const {
    return RoundUpMessageAlignment(sizeof(*this) + size_);
  }

  // TODO(vtl): Add whatever's necessary to transport handles.

  // Messages (the header and data) must always be aligned to a multiple of this
  // quantity (which must be a power of 2).
  static const size_t kMessageAlignment = 8;

  // Rounds |n| up to a multiple of |kMessageAlignment|.
  static inline size_t RoundUpMessageAlignment(size_t n) {
    return (n + kMessageAlignment - 1) & ~(kMessageAlignment - 1);
  }

 private:
  explicit MessageInTransit(uint32_t size)
      : size_(size), reserved_(0), user_1_(0), user_2_(0) {}

  // "Header" for the data.
  uint32_t size_;
  uint32_t reserved_;
  uint32_t user_1_;
  uint32_t user_2_;

  DISALLOW_COPY_AND_ASSIGN(MessageInTransit);
};

// The size of |MessageInTransit| must be appropriate to maintain alignment of
// the following data.
COMPILE_ASSERT(sizeof(MessageInTransit) == 16, MessageInTransit_has_wrong_size);

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_
