// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_
#define MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_

#include <stdint.h>

#include "base/macros.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// This class is used to represent data in transit. It is thread-unsafe.
// Note: This class is POD.
class MOJO_SYSTEM_IMPL_EXPORT MessageInTransit {
 public:
  typedef uint16_t Type;
  // Messages that are forwarded to |MessagePipeEndpoint|s.
  static const Type kTypeMessagePipeEndpoint = 0;
  // Messages that are forwarded to |MessagePipe|s.
  static const Type kTypeMessagePipe = 1;
  // Messages that are consumed by the channel.
  static const Type kTypeChannel = 2;

  typedef uint16_t Subtype;
  // Subtypes for type |kTypeMessagePipeEndpoint|:
  static const Subtype kSubtypeMessagePipeEndpointData = 0;
  // Subtypes for type |kTypeMessagePipe|:
  static const Subtype kSubtypeMessagePipePeerClosed = 0;

  typedef uint32_t EndpointId;
  // Never a valid endpoint ID.
  static const EndpointId kInvalidEndpointId = 0;

  // Messages (the header and data) must always be aligned to a multiple of this
  // quantity (which must be a power of 2).
  static const size_t kMessageAlignment = 8;

  // Creates a |MessageInTransit| of the given |type| and |subtype|, with
  // message data given by |bytes|/|num_bytes|.
  // TODO(vtl): Add ability to tack on handle information.
  static MessageInTransit* Create(Type type,
                                  Subtype subtype,
                                  const void* bytes,
                                  uint32_t num_bytes,
                                  uint32_t num_handles);

  MessageInTransit* Clone() const;

  // Destroys a |MessageInTransit| created using |Create()| or |Clone()|.
  void Destroy();

  // Gets the "main" buffer for a |MessageInTransit|. A |MessageInTransit| can
  // be serialized by writing the main buffer. The returned pointer will be
  // aligned to a multiple of |kMessageAlignment| bytes, and the size of the
  // buffer (see below) will also be a multiple of |kMessageAlignment|.
  // TODO(vtl): Add a "secondary" buffer, so that this makes more sense.
  const void* main_buffer() const {
    return static_cast<const void*>(this);
  }

  // Gets the size of the main buffer (in number of bytes).
  size_t main_buffer_size() const {
    return RoundUpMessageAlignment(sizeof(*this) + header()->data_size);
  }

  // Gets the size of the data (in number of bytes). This is the full size of
  // the data that follows the header, and may include data other than the
  // message data. (See also |num_bytes()|.)
  uint32_t data_size() const {
    return header()->data_size;
  }

  // Gets the data (of size |data_size()| bytes).
  const void* data() const {
    return reinterpret_cast<const char*>(this) + sizeof(*this);
  }

  // Gets the size of the message data.
  uint32_t num_bytes() const {
    return header()->num_bytes;
  }

  // Gets the message data (of size |num_bytes()| bytes).
  const void* bytes() const {
    return reinterpret_cast<const char*>(this) + sizeof(*this);
  }

  uint32_t num_handles() const {
    return header()->num_handles;
  }

  Type type() const { return header()->type; }
  Subtype subtype() const { return header()->subtype; }
  EndpointId source_id() const { return header()->source_id; }
  EndpointId destination_id() const { return header()->destination_id; }

  void set_source_id(EndpointId source_id) { header()->source_id = source_id; }
  void set_destination_id(EndpointId destination_id) {
    header()->destination_id = destination_id;
  }

  // TODO(vtl): Add whatever's necessary to transport handles.

  // Rounds |n| up to a multiple of |kMessageAlignment|.
  static inline size_t RoundUpMessageAlignment(size_t n) {
    return (n + kMessageAlignment - 1) & ~(kMessageAlignment - 1);
  }

 private:
  // "Header" for the data. Must be a multiple of |kMessageAlignment| bytes in
  // size.
  struct Header {
    Header(uint32_t data_size, Type type, Subtype subtype, EndpointId source_id,
           EndpointId destination_id, uint32_t num_bytes, uint32_t num_handles)
        : data_size(data_size), type(type), subtype(subtype),
          source_id(source_id), destination_id(destination_id),
          num_bytes(num_bytes), num_handles(num_handles), reserved0(0),
          reserved1(0) {}

    // Total size of data following the "header".
    uint32_t data_size;
    Type type;
    Subtype subtype;
    EndpointId source_id;
    EndpointId destination_id;
    // Size of actual message data.
    uint32_t num_bytes;
    // Number of handles "attached".
    uint32_t num_handles;
    // To be used soon.
    uint32_t reserved0;
    uint32_t reserved1;
  };

  MessageInTransit(uint32_t data_size,
                   Type type,
                   Subtype subtype,
                   uint32_t num_bytes,
                   uint32_t num_handles);

  const Header* header() const { return &header_; }
  Header* header() { return &header_; }

  Header header_;

  // Intentionally unimplemented (and private): Use |Destroy()| instead (which
  // simply frees the memory).
  ~MessageInTransit();

  DISALLOW_COPY_AND_ASSIGN(MessageInTransit);
};

// The size of |MessageInTransit| must be appropriate to maintain alignment of
// the following data.
COMPILE_ASSERT(sizeof(MessageInTransit) % MessageInTransit::kMessageAlignment ==
                   0, MessageInTransit_has_wrong_size);

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_
