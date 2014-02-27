// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_in_transit.h"

#include <string.h>

#include <new>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "mojo/system/constants.h"

namespace mojo {
namespace system {

struct MessageInTransit::PrivateStructForCompileAsserts {
  // The size of |Header| must be appropriate to maintain alignment of the
  // following data.
  COMPILE_ASSERT(sizeof(Header) % kMessageAlignment == 0,
                 sizeof_MessageInTransit_Header_invalid);
  // Avoid dangerous situations, but making sure that the size of the "header" +
  // the size of the data fits into a 31-bit number.
  COMPILE_ASSERT(static_cast<uint64_t>(sizeof(Header)) + kMaxMessageNumBytes <=
                     0x7fffffffULL,
                 kMaxMessageNumBytes_too_big);

  // The size of |HandleTableEntry| must be appropriate to maintain alignment.
  COMPILE_ASSERT(sizeof(HandleTableEntry) % kMessageAlignment == 0,
                 sizeof_MessageInTransit_HandleTableEntry_invalid);
};

STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Type
    MessageInTransit::kTypeMessagePipeEndpoint;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Type
    MessageInTransit::kTypeMessagePipe;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Type
    MessageInTransit::kTypeChannel;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Subtype
    MessageInTransit::kSubtypeMessagePipeEndpointData;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Subtype
    MessageInTransit::kSubtypeMessagePipePeerClosed;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::EndpointId
    MessageInTransit::kInvalidEndpointId;
STATIC_CONST_MEMBER_DEFINITION const size_t MessageInTransit::kMessageAlignment;

MessageInTransit::MessageInTransit(OwnedBuffer,
                                   Type type,
                                   Subtype subtype,
                                   uint32_t num_bytes,
                                   uint32_t num_handles,
                                   const void* bytes)
    : owns_buffers_(true),
      main_buffer_size_(RoundUpMessageAlignment(sizeof(Header) + num_bytes)),
      main_buffer_(base::AlignedAlloc(main_buffer_size_, kMessageAlignment)),
      secondary_buffer_size_(0),
      secondary_buffer_(NULL) {
  DCHECK_LE(num_bytes, kMaxMessageNumBytes);
  DCHECK_LE(num_handles, kMaxMessageNumHandles);

  // |total_size| is updated below, from the other values.
  header()->type = type;
  header()->subtype = subtype;
  header()->source_id = kInvalidEndpointId;
  header()->destination_id = kInvalidEndpointId;
  header()->num_bytes = num_bytes;
  header()->num_handles = num_handles;
  header()->reserved0 = 0;
  header()->reserved1 = 0;
  // Note: If dispatchers are subsequently attached (in particular, if
  // |num_handles| is nonzero), then |total_size| will have to be adjusted.
  UpdateTotalSize();

  if (bytes) {
    memcpy(MessageInTransit::bytes(), bytes, num_bytes);
    memset(static_cast<char*>(MessageInTransit::bytes()) + num_bytes, 0,
           main_buffer_size_ - sizeof(Header) - num_bytes);
  } else {
    memset(MessageInTransit::bytes(), 0, main_buffer_size_ - sizeof(Header));
  }
}

MessageInTransit::MessageInTransit(OwnedBuffer,
                                   const MessageInTransit& other)
    : owns_buffers_(true),
      main_buffer_size_(other.main_buffer_size()),
      main_buffer_(base::AlignedAlloc(main_buffer_size_, kMessageAlignment)),
      secondary_buffer_size_(other.secondary_buffer_size()),
      secondary_buffer_(secondary_buffer_size_ ?
                            base::AlignedAlloc(secondary_buffer_size_,
                                               kMessageAlignment) : NULL) {
  DCHECK(!other.dispatchers_.get());
  DCHECK_GE(main_buffer_size_, sizeof(Header));
  DCHECK_EQ(main_buffer_size_ % kMessageAlignment, 0u);

  memcpy(main_buffer_, other.main_buffer(), main_buffer_size_);
  memcpy(secondary_buffer_, other.secondary_buffer(), secondary_buffer_size_);

  DCHECK_EQ(main_buffer_size_,
            RoundUpMessageAlignment(sizeof(Header) + num_bytes()));
}

MessageInTransit::MessageInTransit(UnownedBuffer,
                                   size_t message_size,
                                   void* buffer)
    : owns_buffers_(false),
      main_buffer_size_(0),
      main_buffer_(NULL),
      secondary_buffer_size_(0),
      secondary_buffer_(NULL) {
  DCHECK_GE(message_size, sizeof(Header));
  DCHECK_EQ(message_size % kMessageAlignment, 0u);
  DCHECK(buffer);

  Header* header = static_cast<Header*>(buffer);
  DCHECK_EQ(header->total_size, message_size);

  main_buffer_size_ =
      RoundUpMessageAlignment(sizeof(Header) + header->num_bytes);
  DCHECK_LE(main_buffer_size_, message_size);
  main_buffer_ = buffer;
  DCHECK_EQ(reinterpret_cast<uintptr_t>(main_buffer_) % kMessageAlignment, 0u);

  if (message_size > main_buffer_size_) {
    secondary_buffer_size_ = message_size - main_buffer_size_;
    secondary_buffer_ = static_cast<char*>(buffer) + main_buffer_size_;
    DCHECK_EQ(reinterpret_cast<uintptr_t>(secondary_buffer_) %
                  kMessageAlignment, 0u);
  }
}

MessageInTransit::~MessageInTransit() {
  if (owns_buffers_) {
    base::AlignedFree(main_buffer_);
    base::AlignedFree(secondary_buffer_);  // Okay if null.
#ifndef NDEBUG
    main_buffer_size_ = 0;
    main_buffer_ = NULL;
    secondary_buffer_size_ = 0;
    secondary_buffer_ = NULL;
#endif
  }

  if (dispatchers_.get()) {
    for (size_t i = 0; i < dispatchers_->size(); i++) {
      if (!(*dispatchers_)[i])
        continue;

      DCHECK((*dispatchers_)[i]->HasOneRef());
      (*dispatchers_)[i]->Close();
    }
    dispatchers_.reset();
  }
}

// static
bool MessageInTransit::GetNextMessageSize(const void* buffer,
                                          size_t buffer_size,
                                          size_t* next_message_size) {
  DCHECK(buffer);
  DCHECK_EQ(reinterpret_cast<uintptr_t>(buffer) %
                MessageInTransit::kMessageAlignment, 0u);
  DCHECK(next_message_size);

  if (buffer_size < sizeof(Header))
    return false;

  const Header* header = static_cast<const Header*>(buffer);
  *next_message_size = header->total_size;
  DCHECK_EQ(*next_message_size % kMessageAlignment, 0u);
  return true;
}

void MessageInTransit::SetDispatchers(
    scoped_ptr<std::vector<scoped_refptr<Dispatcher> > > dispatchers) {
  DCHECK(dispatchers.get());
  DCHECK(owns_buffers_);
  DCHECK(!dispatchers_.get());

  dispatchers_ = dispatchers.Pass();
#ifndef NDEBUG
  for (size_t i = 0; i < dispatchers_->size(); i++)
    DCHECK(!(*dispatchers_)[i] || (*dispatchers_)[i]->HasOneRef());
#endif
}

void MessageInTransit::SerializeAndCloseDispatchers(Channel* channel) {
  DCHECK(channel);
  DCHECK(owns_buffers_);
  DCHECK(!secondary_buffer_);
  CHECK_EQ(num_handles(),
           dispatchers_.get() ? dispatchers_->size() : static_cast<size_t>(0));

  if (!num_handles())
    return;

  size_t handle_table_size = num_handles() * sizeof(HandleTableEntry);
  // The size of the secondary buffer. We'll start with the size of the handle
  // table, and add to it as we go along.
  size_t size = handle_table_size;
  for (size_t i = 0; i < dispatchers_->size(); i++) {
    if (!(*dispatchers_)[i])
      continue;

    size += RoundUpMessageAlignment(
        Dispatcher::MessageInTransitAccess::GetMaximumSerializedSize(
            (*dispatchers_)[i].get(), channel));
    // TODO(vtl): Check for overflow?
  }

  secondary_buffer_ = base::AlignedAlloc(size, kMessageAlignment);
  // TODO(vtl): Check for overflow?
  secondary_buffer_size_ = static_cast<uint32_t>(size);
  // Entirely clear out the secondary buffer, since then we won't have to worry
  // about clearing padding or unused space (e.g., if a dispatcher fails to
  // serialize).
  memset(secondary_buffer_, 0, size);

  HandleTableEntry* handle_table =
      static_cast<HandleTableEntry*>(secondary_buffer_);
  size_t current_offset = handle_table_size;
  for (size_t i = 0; i < dispatchers_->size(); i++) {
    if (!(*dispatchers_)[i]) {
      // The |handle_table[i].size| is already zero, designating this entry as
      // invalid.
      continue;
    }

    handle_table[i].offset = static_cast<uint32_t>(current_offset);
    handle_table[i].size = static_cast<uint32_t>(
        Dispatcher::MessageInTransitAccess::SerializeAndClose(
            (*dispatchers_)[i].get(),
            static_cast<char*>(secondary_buffer_) + current_offset,
            channel));
    current_offset += RoundUpMessageAlignment(handle_table[i].size);
    DCHECK_LE(current_offset, size);
  }

  UpdateTotalSize();
}

void MessageInTransit::UpdateTotalSize() {
  DCHECK_EQ(main_buffer_size_ % kMessageAlignment, 0u);
  DCHECK_EQ(secondary_buffer_size_ % kMessageAlignment, 0u);
  header()->total_size =
      static_cast<uint32_t>(main_buffer_size_ + secondary_buffer_size_);
}

}  // namespace system
}  // namespace mojo
