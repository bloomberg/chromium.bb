// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_INTERNAL_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"

namespace mojo {

class Message;

namespace internal {

template <typename T>
class Array_Data;

#pragma pack(push, 1)

struct MessageHeader : internal::StructHeader {
  // Interface ID for identifying multiple interfaces running on the same
  // message pipe.
  uint32_t interface_id;
  // Message name, which is scoped to the interface that the message belongs to.
  uint32_t name;
  // 0 or either of the enum values defined above.
  uint32_t flags;
  // Unused padding to make the struct size a multiple of 8 bytes.
  uint32_t padding;
};
static_assert(sizeof(MessageHeader) == 24, "Bad sizeof(MessageHeader)");

struct MessageHeaderV1 : MessageHeader {
  // Only used if either kFlagExpectsResponse or kFlagIsResponse is set in
  // order to match responses with corresponding requests.
  uint64_t request_id;
};
static_assert(sizeof(MessageHeaderV1) == 32, "Bad sizeof(MessageHeaderV1)");

struct MessageHeaderV2 : MessageHeaderV1 {
  MessageHeaderV2();
  GenericPointer payload;
  Pointer<Array_Data<uint32_t>> payload_interface_ids;
};
static_assert(sizeof(MessageHeaderV2) == 48, "Bad sizeof(MessageHeaderV2)");

#pragma pack(pop)

class MOJO_CPP_BINDINGS_EXPORT MessageDispatchContext {
 public:
  explicit MessageDispatchContext(Message* message);
  ~MessageDispatchContext();

  static MessageDispatchContext* current();

  const base::Callback<void(const std::string&)>& GetBadMessageCallback();

 private:
  MessageDispatchContext* outer_context_;
  Message* message_;
  base::Callback<void(const std::string&)> bad_message_callback_;

  DISALLOW_COPY_AND_ASSIGN(MessageDispatchContext);
};

class MOJO_CPP_BINDINGS_EXPORT SyncMessageResponseSetup {
 public:
  static void SetCurrentSyncResponseMessage(Message* message);
};

MOJO_CPP_BINDINGS_EXPORT size_t
ComputeSerializedMessageSize(uint32_t flags,
                             size_t payload_size,
                             size_t payload_interface_id_count);

// Used by generated bindings to bypass validation for unserialized message
// objects and control messages.
MOJO_CPP_BINDINGS_EXPORT bool IsUnserializedOrControlMessage(Message* message);

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MESSAGE_INTERNAL_H_
