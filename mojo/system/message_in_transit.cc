// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_in_transit.h"

#include <stdlib.h>
#include <string.h>

#include <new>

#include "base/basictypes.h"
#include "mojo/system/limits.h"

namespace mojo {
namespace system {

// Avoid dangerous situations, but making sure that the size of the "header" +
// the size of the data fits into a 31-bit number.
COMPILE_ASSERT(static_cast<uint64_t>(sizeof(MessageInTransit)) +
                   kMaxMessageNumBytes <= 0x7fffffff,
               kMaxMessageNumBytes_too_big);

// static
MessageInTransit* MessageInTransit::Create(const void* bytes,
                                           uint32_t num_bytes) {
  // Store the data immediately after the "header", so allocate the needed
  // space.
  char* buffer = static_cast<char*>(
      malloc(sizeof(MessageInTransit) + static_cast<size_t>(num_bytes)));
  // And do a placement new.
  MessageInTransit* rv = new (buffer) MessageInTransit(num_bytes);
  memcpy(buffer + sizeof(MessageInTransit), bytes, num_bytes);
  return rv;
}

}  // namespace system
}  // namespace mojo
