// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_header_validator.h"

#include "mojo/public/cpp/bindings/lib/bindings_serialization.h"

namespace mojo {
namespace internal {
namespace {

bool IsValidMessageHeader(const internal::MessageHeader* header) {
  // NOTE: Our goal is to preserve support for future extension of the message
  // header. If we encounter fields we do not understand, we must ignore them.

  // Validate num_bytes:

  if (header->num_bytes < sizeof(internal::MessageHeader))
    return false;
  if (internal::Align(header->num_bytes) != header->num_bytes)
    return false;

  // Validate num_fields:

  if (header->num_fields < 2)
    return false;
  if (header->num_fields == 2) {
    if (header->num_bytes != sizeof(internal::MessageHeader))
      return false;
  } else if (header->num_fields == 3) {
    if (header->num_bytes != sizeof(internal::MessageHeaderWithRequestID))
      return false;
  } else if (header->num_fields > 3) {
    if (header->num_bytes < sizeof(internal::MessageHeaderWithRequestID))
      return false;
  }

  // Validate flags (allow unknown bits):

  // These flags require a RequestID.
  if (header->num_fields < 3 &&
        ((header->flags & internal::kMessageExpectsResponse) ||
         (header->flags & internal::kMessageIsResponse)))
    return false;

  // These flags are mutually exclusive.
  if ((header->flags & internal::kMessageExpectsResponse) &&
      (header->flags & internal::kMessageIsResponse))
    return false;

  return true;
}

}  // namespace

MessageHeaderValidator::MessageHeaderValidator(MessageReceiver* next)
    : next_(next) {
  assert(next);
}

bool MessageHeaderValidator::Accept(Message* message) {
  // Make sure the message header isn't truncated before we start to read it.
  if (message->data_num_bytes() < message->header()->num_bytes)
    return false;

  if (!IsValidMessageHeader(message->header()))
    return false;

  return next_->Accept(message);
}

bool MessageHeaderValidator::AcceptWithResponder(Message* message,
                                                 MessageReceiver* responder) {
  assert(false);  // Not reached!
  return false;
}

}  // namespace internal
}  // namespace mojo
