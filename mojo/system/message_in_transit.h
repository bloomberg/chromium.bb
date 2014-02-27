// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_
#define MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class Channel;

// This class is used to represent data in transit. It is thread-unsafe.
//
// |MessageInTransit| buffers:
//
// A |MessageInTransit| can be serialized by writing the main buffer and then,
// if it has one, the secondary buffer. Both buffers are
// |kMessageAlignment|-byte aligned and a multiple of |kMessageAlignment| bytes
// in size.
//
// The main buffer consists of the header (of type |Header|, which is an
// internal detail of this class) followed immediately by the message data
// (accessed by |bytes()| and of size |num_bytes()|, and also
// |kMessageAlignment|-byte aligned), and then any padding needed to make the
// main buffer a multiple of |kMessageAlignment| bytes in size.
//
// The secondary buffer consists first of a table of |HandleTableEntry|s (each
// of which is already a multiple of |kMessageAlignment| in size), followed by
// data needed for the |HandleTableEntry|s: A |HandleTableEntry| consists of an
// offset (in bytes, relative to the start of the secondary buffer; we guarantee
// that it's a multiple of |kMessageAlignment|), and a size (in bytes).
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

  enum OwnedBuffer { OWNED_BUFFER };
  // Constructor for a |MessageInTransit| that owns its buffer. |bytes| is
  // optional; if null, the message data will be zero-initialized.
  MessageInTransit(OwnedBuffer,
                   Type type,
                   Subtype subtype,
                   uint32_t num_bytes,
                   uint32_t num_handles,
                   const void* bytes);
  // "Copy" constructor. The input |MessageInTransit| may own its buffer or not;
  // however, it must not have any dispatchers. The constructed
  // |MessageInTransit| will own its buffer.
  MessageInTransit(OwnedBuffer,
                   const MessageInTransit& other);

  enum UnownedBuffer { UNOWNED_BUFFER };
  // Constructor for a |MessageInTransit| that is a "view" into another buffer.
  // |buffer| should point to a fully-serialized |MessageInTransit|, and should
  // be aligned on a |kMessageAlignment|-byte boundary. |message_size| should be
  // the value provided by |GetNextMessageSize()|, and |buffer| should have at
  // least that many bytes available. |buffer| should live (without change to
  // the first |message_size| bytes) at least as long the new |MessageInTransit|
  // does.
  //
  // Note: You probably don't want to heap-allocate this kind of
  // |MessageInTransit| (and, e.g., put it into a |scoped_ptr|); you definitely
  // don't want to pass it as a parameter in a |scoped_ptr|. Whenever you use
  // this, you can probably create it directly on the stack.
  MessageInTransit(UnownedBuffer, size_t message_size, void* buffer);
  ~MessageInTransit();

  // Gets the size of the next message from |buffer|, which has |buffer_size|
  // bytes currently available, returning true and setting |*next_message_size|
  // on success. |buffer| should be aligned on a |kMessageAlignment| boundary
  // (and on success, |*next_message_size| will be a multiple of
  // |kMessageAlignment|).
  // TODO(vtl): In |RawChannelPosix|, the alignment requirements are currently
  // satisified on a faith-based basis.
  static bool GetNextMessageSize(const void* buffer,
                                 size_t buffer_size,
                                 size_t* next_message_size);

  // Makes this message "own" the given set of dispatchers. The dispatchers must
  // not be referenced from anywhere else (in particular, not from the handle
  // table), i.e., each dispatcher must have a reference count of 1. This
  // message must also own its buffers and not already have dispatchers.
  void SetDispatchers(
      scoped_ptr<std::vector<scoped_refptr<Dispatcher> > > dispatchers);

  // Serializes any dispatchers to the secondary buffer. This message must own
  // its buffers (in order to have dispatchers in the first place), and the
  // secondary buffer must not yet exist (so this must only be called once). The
  // caller must ensure (e.g., by holding on to a reference) that |channel|
  // stays alive through the call.
  void SerializeAndCloseDispatchers(Channel* channel);

  // Gets the main buffer and its size (in number of bytes), respectively.
  const void* main_buffer() const { return main_buffer_; }
  size_t main_buffer_size() const { return main_buffer_size_; }

  // Gets the secondary buffer and its size (in number of bytes), respectively.
  const void* secondary_buffer() const { return secondary_buffer_; }
  size_t secondary_buffer_size() const { return secondary_buffer_size_; }

  // Gets the total size of the message (see comment in |Header|, below).
  size_t total_size() const { return header()->total_size; }

  // Gets the size of the message data.
  uint32_t num_bytes() const { return header()->num_bytes; }

  // Gets the message data (of size |num_bytes()| bytes).
  const void* bytes() const {
    return static_cast<const char*>(main_buffer_) + sizeof(Header);
  }
  void* bytes() { return static_cast<char*>(main_buffer_) + sizeof(Header); }

  uint32_t num_handles() const { return header()->num_handles; }

  Type type() const { return header()->type; }
  Subtype subtype() const { return header()->subtype; }
  EndpointId source_id() const { return header()->source_id; }
  EndpointId destination_id() const { return header()->destination_id; }

  void set_source_id(EndpointId source_id) { header()->source_id = source_id; }
  void set_destination_id(EndpointId destination_id) {
    header()->destination_id = destination_id;
  }

  // Gets the dispatchers attached to this message; this may return null if
  // there are none. Note that the caller may mutate the set of dispatchers
  // (e.g., take ownership of all the dispatchers, leaving the vector empty).
  std::vector<scoped_refptr<Dispatcher> >* dispatchers() {
    return dispatchers_.get();
  }

  // Rounds |n| up to a multiple of |kMessageAlignment|.
  static inline size_t RoundUpMessageAlignment(size_t n) {
    return (n + kMessageAlignment - 1) & ~(kMessageAlignment - 1);
  }

 private:
  // To allow us to make assertions about |Header| in the .cc file.
  struct PrivateStructForCompileAsserts;

  // "Header" for the data. Must be a multiple of |kMessageAlignment| bytes in
  // size. Must be POD.
  struct Header {
    // Total size of the message, including the header, the message data
    // ("bytes") including padding (to make it a multiple of |kMessageAlignment|
    // bytes), and serialized handle information. Note that this may not be the
    // correct value if dispatchers are attached but
    // |SerializeAndCloseDispatchers()| has not been called.
    uint32_t total_size;
    Type type;  // 2 bytes.
    Subtype subtype;  // 2 bytes.
    EndpointId source_id;  // 4 bytes.
    EndpointId destination_id;  // 4 bytes.
    // Size of actual message data.
    uint32_t num_bytes;
    // Number of handles "attached".
    uint32_t num_handles;
    // To be used soon.
    uint32_t reserved0;
    uint32_t reserved1;
  };

  struct HandleTableEntry {
    uint32_t offset;
    uint32_t size;  // (Not including any padding.) A size of 0 means "invalid".
  };

  const Header* header() const {
    return static_cast<const Header*>(main_buffer_);
  }
  Header* header() { return static_cast<Header*>(main_buffer_); }

  void UpdateTotalSize();

  // Whether we own |main_buffer_| and |secondary_buffer_| or not (that is, we
  // own neither).
  bool owns_buffers_;

  size_t main_buffer_size_;
  void* main_buffer_;

  size_t secondary_buffer_size_;
  void* secondary_buffer_;  // May be null.

  // Any dispatchers that may be attached to this message. This is only
  // supported if this message owns its main buffer. These dispatchers should be
  // "owned" by this message, i.e., have a ref count of exactly 1. (We allow a
  // dispatcher entry to be null, in case it couldn't be duplicated for some
  // reason.)
  scoped_ptr<std::vector<scoped_refptr<Dispatcher> > > dispatchers_;

  DISALLOW_COPY_AND_ASSIGN(MessageInTransit);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MESSAGE_IN_TRANSIT_H_
