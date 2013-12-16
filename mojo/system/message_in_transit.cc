// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_in_transit.h"

#include <stdlib.h>
#include <string.h>

#include <new>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "mojo/system/constants.h"

namespace mojo {
namespace system {

// Avoid dangerous situations, but making sure that the size of the "header" +
// the size of the data fits into a 31-bit number.
COMPILE_ASSERT(static_cast<uint64_t>(sizeof(MessageInTransit)) +
                   kMaxMessageNumBytes <= 0x7fffffff,
               kMaxMessageNumBytes_too_big);

COMPILE_ASSERT(sizeof(MessageInTransit) %
                   MessageInTransit::kMessageAlignment == 0,
               sizeof_MessageInTransit_not_a_multiple_of_alignment);

STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Type
    MessageInTransit::kTypeMessagePipeEndpoint;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Type
    MessageInTransit::kTypeMessagePipe;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Type
    MessageInTransit::TYPE_CHANNEL;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Subtype
    MessageInTransit::kSubtypeMessagePipeEndpointData;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Subtype
    MessageInTransit::kSubtypeMessagePipePeerClosed;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::EndpointId
    MessageInTransit::kInvalidEndpointId;
STATIC_CONST_MEMBER_DEFINITION const size_t MessageInTransit::kMessageAlignment;

// static
MessageInTransit* MessageInTransit::Create(Type type,
                                           Subtype subtype,
                                           const void* bytes,
                                           uint32_t num_bytes) {
  const size_t size_with_header = sizeof(MessageInTransit) + num_bytes;
  const size_t size_with_header_and_padding =
      RoundUpMessageAlignment(size_with_header);

  char* buffer = static_cast<char*>(malloc(size_with_header_and_padding));
  DCHECK_EQ(reinterpret_cast<size_t>(buffer) %
                MessageInTransit::kMessageAlignment, 0u);

  // The buffer consists of the header (a |MessageInTransit|, constructed using
  // a placement new), followed by the data, followed by padding (of zeros).
  MessageInTransit* rv =
      new (buffer) MessageInTransit(num_bytes, type, subtype);
  memcpy(buffer + sizeof(MessageInTransit), bytes, num_bytes);
  memset(buffer + size_with_header, 0,
         size_with_header_and_padding - size_with_header);
  return rv;
}

}  // namespace system
}  // namespace mojo
