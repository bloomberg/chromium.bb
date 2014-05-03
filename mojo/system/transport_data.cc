// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/transport_data.h"

#include <string.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "mojo/system/constants.h"
#include "mojo/system/message_in_transit.h"

namespace mojo {
namespace system {

STATIC_CONST_MEMBER_DEFINITION const size_t
    TransportData::kMaxSerializedDispatcherSize;
STATIC_CONST_MEMBER_DEFINITION const size_t
    TransportData::kMaxSerializedDispatcherPlatformHandles;

// In additional to the header, for each attached (Mojo) handle there'll be a
// handle table entry and serialized dispatcher data.
// static
const size_t TransportData::kMaxBufferSize =
    sizeof(Header) + kMaxMessageNumHandles * (sizeof(HandleTableEntry) +
                                              kMaxSerializedDispatcherSize);

// static
const size_t TransportData::kMaxPlatformHandles =
    kMaxMessageNumHandles * kMaxSerializedDispatcherPlatformHandles;

struct TransportData::PrivateStructForCompileAsserts {
  // The size of |Header| must be a multiple of the alignment.
  COMPILE_ASSERT(sizeof(Header) % MessageInTransit::kMessageAlignment == 0,
                 sizeof_MessageInTransit_Header_invalid);

  // The maximum serialized dispatcher size must be a multiple of the alignment.
  COMPILE_ASSERT(kMaxSerializedDispatcherSize %
                     MessageInTransit::kMessageAlignment == 0,
                 kMaxSerializedDispatcherSize_not_a_multiple_of_alignment);

  // The size of |HandleTableEntry| must be a multiple of the alignment.
  COMPILE_ASSERT(sizeof(HandleTableEntry) %
                     MessageInTransit::kMessageAlignment == 0,
                 sizeof_MessageInTransit_HandleTableEntry_invalid);
};

TransportData::TransportData(
    scoped_ptr<std::vector<scoped_refptr<Dispatcher> > > dispatchers,
    Channel* channel)
    : buffer_size_(0) {
  DCHECK(dispatchers);
  DCHECK(channel);

  const size_t num_handles = dispatchers->size();
  DCHECK_GT(num_handles, 0u);

  // The offset to the start of the (Mojo) handle table.
  const size_t handle_table_start_offset = sizeof(Header);
  // The offset to the start of the serialized dispatcher data.
  const size_t serialized_dispatcher_start_offset =
      handle_table_start_offset + num_handles * sizeof(HandleTableEntry);
  // The estimated size of the secondary buffer. We compute this estimate below.
  // It must be at least as big as the (eventual) actual size.
  size_t estimated_size = serialized_dispatcher_start_offset;
  size_t num_platform_handles = 0;
#if DCHECK_IS_ON
  std::vector<size_t> all_max_sizes(num_handles);
  std::vector<size_t> all_max_platform_handles(num_handles);
#endif
  for (size_t i = 0; i < num_handles; i++) {
    if (Dispatcher* dispatcher = (*dispatchers)[i].get()) {
      size_t max_size = 0;
      size_t max_platform_handles = 0;
      Dispatcher::TransportDataAccess::StartSerialize(
              dispatcher, channel, &max_size, &max_platform_handles);

      DCHECK_LE(max_size, kMaxSerializedDispatcherSize);
      estimated_size += MessageInTransit::RoundUpMessageAlignment(max_size);
      DCHECK_LE(estimated_size, kMaxBufferSize);

      DCHECK_LE(max_platform_handles,
                kMaxSerializedDispatcherPlatformHandles);
      num_platform_handles += max_platform_handles;
      DCHECK_LE(num_platform_handles, kMaxPlatformHandles);

#if DCHECK_IS_ON
      all_max_sizes[i] = max_size;
      all_max_platform_handles[i] = max_platform_handles;
#endif
    }
  }

  buffer_.reset(static_cast<char*>(
      base::AlignedAlloc(estimated_size, MessageInTransit::kMessageAlignment)));
  // Entirely clear out the secondary buffer, since then we won't have to worry
  // about clearing padding or unused space (e.g., if a dispatcher fails to
  // serialize).
  memset(buffer_.get(), 0, estimated_size);

  if (num_platform_handles > 0) {
    DCHECK(!platform_handles_);
    platform_handles_.reset(new std::vector<embedder::PlatformHandle>());
  }

  Header* header = reinterpret_cast<Header*>(buffer_.get());
  header->num_handles = static_cast<uint32_t>(num_handles);
  // TODO(vtl): platform_handle_table_offset and num_platform_handles

  HandleTableEntry* handle_table = reinterpret_cast<HandleTableEntry*>(
      buffer_.get() + handle_table_start_offset);
  size_t current_offset = serialized_dispatcher_start_offset;
  for (size_t i = 0; i < num_handles; i++) {
    Dispatcher* dispatcher = (*dispatchers)[i].get();
    if (!dispatcher) {
      COMPILE_ASSERT(Dispatcher::kTypeUnknown == 0,
                     value_of_Dispatcher_kTypeUnknown_must_be_zero);
      continue;
    }

#if DCHECK_IS_ON
    size_t old_platform_handles_size =
        platform_handles_ ? platform_handles_->size() : 0;
#endif

    void* destination = buffer_.get() + current_offset;
    size_t actual_size = 0;
    if (Dispatcher::TransportDataAccess::EndSerializeAndClose(
            dispatcher, channel, destination, &actual_size,
            platform_handles_.get())) {
      handle_table[i].type = static_cast<int32_t>(dispatcher->GetType());
      handle_table[i].offset = static_cast<uint32_t>(current_offset);
      handle_table[i].size = static_cast<uint32_t>(actual_size);

#if DCHECK_IS_ON
      DCHECK_LE(actual_size, all_max_sizes[i]);
      DCHECK_LE(platform_handles_ ? (platform_handles_->size() -
                                         old_platform_handles_size) : 0,
                all_max_platform_handles[i]);
#endif
    } else {
      // Nothing to do on failure, since |buffer_| was cleared, and
      // |kTypeUnknown| is zero. The handle was simply closed.
      LOG(ERROR) << "Failed to serialize handle to remote message pipe";
    }

    current_offset += MessageInTransit::RoundUpMessageAlignment(actual_size);
    DCHECK_LE(current_offset, estimated_size);
    DCHECK_LE(platform_handles_ ? platform_handles_->size() : 0,
              num_platform_handles);
  }

  // There's no aligned realloc, so it's no good way to release unused space (if
  // we overshot our estimated space requirements).
  buffer_size_ = current_offset;

  // |dispatchers_| will be destroyed as it goes out of scope.
}

TransportData::~TransportData() {
  if (platform_handles_) {
    for (size_t i = 0; i < platform_handles_->size(); i++)
      (*platform_handles_)[i].CloseIfNecessary();
  }
}

// static
const char* TransportData::ValidateBuffer(const void* buffer,
                                          size_t buffer_size) {
  DCHECK(buffer);
  DCHECK_GT(buffer_size, 0u);

  // Always make sure that the buffer size is sane; if it's not, someone's
  // messing with us.
  if (buffer_size < sizeof(Header) || buffer_size > kMaxBufferSize ||
      buffer_size % MessageInTransit::kMessageAlignment != 0)
    return "Invalid message secondary buffer size";

  const Header* header = static_cast<const Header*>(buffer);
  const size_t num_handles = header->num_handles;
  if (num_handles == 0)
    return "Message has no handles attached, but secondary buffer present";

  // Sanity-check |num_handles| (before multiplying it against anything).
  if (num_handles > kMaxMessageNumHandles)
    return "Message handle payload too large";

  if (buffer_size < sizeof(Header) + num_handles * sizeof(HandleTableEntry))
    return "Message secondary buffer too small";

  // TODO(vtl): Check |platform_handle_table_offset| and |num_platform_handles|
  // once they're used.
  if (header->platform_handle_table_offset != 0 ||
      header->num_platform_handles != 0)
    return "Bad message secondary buffer header values";

  const HandleTableEntry* handle_table =
      reinterpret_cast<const HandleTableEntry*>(
          static_cast<const char*>(buffer) + sizeof(Header));
  static const char kInvalidSerializedDispatcher[] =
      "Message contains invalid serialized dispatcher";
  for (size_t i = 0; i < num_handles; i++) {
    size_t offset = handle_table[i].offset;
    if (offset % MessageInTransit::kMessageAlignment != 0)
      return kInvalidSerializedDispatcher;

    size_t size = handle_table[i].size;
    if (size > kMaxSerializedDispatcherSize || size > buffer_size)
      return kInvalidSerializedDispatcher;

    // Note: This is an overflow-safe check for |offset + size > buffer_size|
    // (we know that |size <= buffer_size| from the previous check).
    if (offset > buffer_size - size)
      return kInvalidSerializedDispatcher;
  }

  return NULL;
}

// static
scoped_ptr<std::vector<scoped_refptr<Dispatcher> > >
    TransportData::DeserializeDispatchersFromBuffer(const void* buffer,
                                                    size_t buffer_size,
                                                    Channel* channel) {
  DCHECK(buffer);
  DCHECK_GT(buffer_size, 0u);
  DCHECK(channel);

  const Header* header = static_cast<const Header*>(buffer);
  const size_t num_handles = header->num_handles;
  scoped_ptr<std::vector<scoped_refptr<Dispatcher> > > dispatchers(
      new std::vector<scoped_refptr<Dispatcher> >(num_handles));

  const HandleTableEntry* handle_table =
      reinterpret_cast<const HandleTableEntry*>(
          static_cast<const char*>(buffer) + sizeof(Header));
  for (size_t i = 0; i < num_handles; i++) {
    size_t offset = handle_table[i].offset;
    size_t size = handle_table[i].size;
    // Should already have been checked by |ValidateBuffer()|:
    DCHECK_EQ(offset % MessageInTransit::kMessageAlignment, 0u);
    DCHECK_LE(offset, buffer_size);
    DCHECK_LE(offset + size, buffer_size);

    const void* source = static_cast<const char*>(buffer) + offset;
    (*dispatchers)[i] = Dispatcher::TransportDataAccess::Deserialize(
        channel, handle_table[i].type, source, size);
  }

  return dispatchers.Pass();
}

}  // namespace system
}  // namespace mojo
