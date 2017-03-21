// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/process/process_handle.h"
#include "mojo/edk/embedder/platform_handle.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mach_logging.h"
#elif defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace mojo {
namespace edk {

namespace {

static_assert(
    IsAlignedForChannelMessage(sizeof(Channel::Message::LegacyHeader)),
    "Invalid LegacyHeader size.");

static_assert(IsAlignedForChannelMessage(sizeof(Channel::Message::Header)),
              "Invalid Header size.");

static_assert(sizeof(Channel::Message::LegacyHeader) == 8,
              "LegacyHeader must be 8 bytes on ChromeOS and Android");

static_assert(offsetof(Channel::Message::LegacyHeader, num_bytes) ==
                  offsetof(Channel::Message::Header, num_bytes),
              "num_bytes should be at the same offset in both Header structs.");
static_assert(offsetof(Channel::Message::LegacyHeader, message_type) ==
                  offsetof(Channel::Message::Header, message_type),
              "message_type should be at the same offset in both Header "
              "structs.");

}  // namespace

const size_t kReadBufferSize = 4096;
const size_t kMaxUnusedReadBufferCapacity = 4096;
const size_t kMaxChannelMessageSize = 256 * 1024 * 1024;
const size_t kMaxAttachedHandles = 128;

Channel::Message::Message(size_t payload_size, size_t max_handles)
#if defined(MOJO_EDK_LEGACY_PROTOCOL)
    : Message(payload_size, max_handles, MessageType::NORMAL_LEGACY) {
}
#else
    : Message(payload_size, max_handles, MessageType::NORMAL) {
}
#endif

Channel::Message::Message(size_t payload_size,
                          size_t max_handles,
                          MessageType message_type)
    : max_handles_(max_handles) {
  DCHECK_LE(max_handles_, kMaxAttachedHandles);

  const bool is_legacy_message = (message_type == MessageType::NORMAL_LEGACY);
  size_t extra_header_size = 0;
#if defined(OS_WIN)
  // On Windows we serialize HANDLEs into the extra header space.
  extra_header_size = max_handles_ * sizeof(HandleEntry);
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  // On OSX, some of the platform handles may be mach ports, which are
  // serialised into the message buffer. Since there could be a mix of fds and
  // mach ports, we store the mach ports as an <index, port> pair (of uint32_t),
  // so that the original ordering of handles can be re-created.
  if (max_handles) {
    extra_header_size =
        sizeof(MachPortsExtraHeader) + (max_handles * sizeof(MachPortsEntry));
  }
#endif
  // Pad extra header data to be aliged to |kChannelMessageAlignment| bytes.
  if (!IsAlignedForChannelMessage(extra_header_size)) {
    extra_header_size += kChannelMessageAlignment -
                         (extra_header_size % kChannelMessageAlignment);
  }
  DCHECK(IsAlignedForChannelMessage(extra_header_size));
  const size_t header_size =
      is_legacy_message ? sizeof(LegacyHeader) : sizeof(Header);
  DCHECK(extra_header_size == 0 || !is_legacy_message);

  size_ = header_size + extra_header_size + payload_size;
  data_ = static_cast<char*>(base::AlignedAlloc(size_,
                                                kChannelMessageAlignment));
  // Only zero out the header and not the payload. Since the payload is going to
  // be memcpy'd, zeroing the payload is unnecessary work and a significant
  // performance issue when dealing with large messages. Any sanitizer errors
  // complaining about an uninitialized read in the payload area should be
  // treated as an error and fixed.
  memset(data_, 0, header_size + extra_header_size);

  DCHECK_LE(size_, std::numeric_limits<uint32_t>::max());
  legacy_header()->num_bytes = static_cast<uint32_t>(size_);

  DCHECK_LE(header_size + extra_header_size,
            std::numeric_limits<uint16_t>::max());
  legacy_header()->message_type = message_type;

  if (is_legacy_message) {
    legacy_header()->num_handles = static_cast<uint16_t>(max_handles);
  } else {
    header()->num_header_bytes =
        static_cast<uint16_t>(header_size + extra_header_size);
  }

  if (max_handles_ > 0) {
#if defined(OS_WIN)
    handles_ = reinterpret_cast<HandleEntry*>(mutable_extra_header());
    // Initialize all handles to invalid values.
    for (size_t i = 0; i < max_handles_; ++i)
      handles_[i].handle = base::win::HandleToUint32(INVALID_HANDLE_VALUE);
#elif defined(OS_MACOSX) && !defined(OS_IOS)
    mach_ports_header_ =
        reinterpret_cast<MachPortsExtraHeader*>(mutable_extra_header());
    mach_ports_header_->num_ports = 0;
    // Initialize all handles to invalid values.
    for (size_t i = 0; i < max_handles_; ++i) {
      mach_ports_header_->entries[i] =
          {0, static_cast<uint32_t>(MACH_PORT_NULL)};
    }
#endif
  }
}

Channel::Message::~Message() {
  base::AlignedFree(data_);
}

// static
Channel::MessagePtr Channel::Message::Deserialize(const void* data,
                                                  size_t data_num_bytes) {
  if (data_num_bytes < sizeof(LegacyHeader))
    return nullptr;

  const LegacyHeader* legacy_header =
      reinterpret_cast<const LegacyHeader*>(data);
  if (legacy_header->num_bytes != data_num_bytes) {
    DLOG(ERROR) << "Decoding invalid message: " << legacy_header->num_bytes
                << " != " << data_num_bytes;
    return nullptr;
  }

  const Header* header = nullptr;
  if (legacy_header->message_type == MessageType::NORMAL)
    header = reinterpret_cast<const Header*>(data);

  uint32_t extra_header_size = 0;
  size_t payload_size = 0;
  const char* payload = nullptr;
  if (!header) {
    payload_size = data_num_bytes - sizeof(LegacyHeader);
    payload = static_cast<const char*>(data) + sizeof(LegacyHeader);
  } else {
    if (header->num_bytes < header->num_header_bytes ||
        header->num_header_bytes < sizeof(Header)) {
      DLOG(ERROR) << "Decoding invalid message: " << header->num_bytes << " < "
                  << header->num_header_bytes;
      return nullptr;
    }
    extra_header_size = header->num_header_bytes - sizeof(Header);
    payload_size = data_num_bytes - header->num_header_bytes;
    payload = static_cast<const char*>(data) + header->num_header_bytes;
  }

#if defined(OS_WIN)
  uint32_t max_handles = extra_header_size / sizeof(HandleEntry);
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  if (extra_header_size > 0 &&
      extra_header_size < sizeof(MachPortsExtraHeader)) {
    DLOG(ERROR) << "Decoding invalid message: " << extra_header_size << " < "
                << sizeof(MachPortsExtraHeader);
    return nullptr;
  }
  uint32_t max_handles =
      extra_header_size == 0
          ? 0
          : (extra_header_size - sizeof(MachPortsExtraHeader)) /
                sizeof(MachPortsEntry);
#else
  const uint32_t max_handles = 0;
#endif  // defined(OS_WIN)

  const uint16_t num_handles =
      header ? header->num_handles : legacy_header->num_handles;
  if (num_handles > max_handles || max_handles > kMaxAttachedHandles) {
    DLOG(ERROR) << "Decoding invalid message: " << num_handles << " > "
                << max_handles;
    return nullptr;
  }

  MessagePtr message(
      new Message(payload_size, max_handles, legacy_header->message_type));
  DCHECK_EQ(message->data_num_bytes(), data_num_bytes);

  // Copy all payload bytes.
  if (payload_size)
    memcpy(message->mutable_payload(), payload, payload_size);

  if (header) {
    DCHECK_EQ(message->extra_header_size(), extra_header_size);
    DCHECK_EQ(message->header()->num_header_bytes, header->num_header_bytes);

    if (message->extra_header_size()) {
      // Copy extra header bytes.
      memcpy(message->mutable_extra_header(),
             static_cast<const char*>(data) + sizeof(Header),
             message->extra_header_size());
    }
    message->header()->num_handles = header->num_handles;
  } else {
    message->legacy_header()->num_handles = legacy_header->num_handles;
  }

#if defined(OS_WIN)
  ScopedPlatformHandleVectorPtr handles(new PlatformHandleVector(num_handles));
  for (size_t i = 0; i < num_handles; i++) {
    (*handles)[i].handle =
        base::win::Uint32ToHandle(message->handles_[i].handle);
  }
  message->SetHandles(std::move(handles));
#endif

  return message;
}

const void* Channel::Message::extra_header() const {
  DCHECK(!is_legacy_message());
  return data_ + sizeof(Header);
}

void* Channel::Message::mutable_extra_header() {
  DCHECK(!is_legacy_message());
  return data_ + sizeof(Header);
}

size_t Channel::Message::extra_header_size() const {
  return header()->num_header_bytes - sizeof(Header);
}

void* Channel::Message::mutable_payload() {
  if (is_legacy_message())
    return static_cast<void*>(legacy_header() + 1);
  return data_ + header()->num_header_bytes;
}

const void* Channel::Message::payload() const {
  if (is_legacy_message())
    return static_cast<const void*>(legacy_header() + 1);
  return data_ + header()->num_header_bytes;
}

size_t Channel::Message::payload_size() const {
  if (is_legacy_message())
    return legacy_header()->num_bytes - sizeof(LegacyHeader);
  return size_ - header()->num_header_bytes;
}

size_t Channel::Message::num_handles() const {
  return is_legacy_message() ? legacy_header()->num_handles
                             : header()->num_handles;
}

bool Channel::Message::has_handles() const {
  return (is_legacy_message() ? legacy_header()->num_handles
                              : header()->num_handles) > 0;
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
bool Channel::Message::has_mach_ports() const {
  if (!has_handles())
    return false;

  for (const auto& handle : (*handle_vector_)) {
    if (handle.type == PlatformHandle::Type::MACH ||
        handle.type == PlatformHandle::Type::MACH_NAME) {
      return true;
    }
  }
  return false;
}
#endif

bool Channel::Message::is_legacy_message() const {
  return legacy_header()->message_type == MessageType::NORMAL_LEGACY;
}

Channel::Message::LegacyHeader* Channel::Message::legacy_header() const {
  return reinterpret_cast<LegacyHeader*>(data_);
}

Channel::Message::Header* Channel::Message::header() const {
  DCHECK(!is_legacy_message());
  return reinterpret_cast<Header*>(data_);
}

void Channel::Message::SetHandles(ScopedPlatformHandleVectorPtr new_handles) {
  if (is_legacy_message()) {
    // Old semantics for ChromeOS and Android
    if (legacy_header()->num_handles == 0) {
      CHECK(!new_handles || new_handles->size() == 0);
      return;
    }
    CHECK(new_handles && new_handles->size() == legacy_header()->num_handles);
    std::swap(handle_vector_, new_handles);
    return;
  }

  if (max_handles_ == 0) {
    CHECK(!new_handles || new_handles->size() == 0);
    return;
  }

  CHECK(new_handles && new_handles->size() <= max_handles_);
  header()->num_handles = static_cast<uint16_t>(new_handles->size());
  std::swap(handle_vector_, new_handles);
#if defined(OS_WIN)
  memset(handles_, 0, extra_header_size());
  for (size_t i = 0; i < handle_vector_->size(); i++)
    handles_[i].handle = base::win::HandleToUint32((*handle_vector_)[i].handle);
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX) && !defined(OS_IOS)
  size_t mach_port_index = 0;
  if (mach_ports_header_) {
    for (size_t i = 0; i < max_handles_; ++i) {
      mach_ports_header_->entries[i] =
          {0, static_cast<uint32_t>(MACH_PORT_NULL)};
    }
    for (size_t i = 0; i < handle_vector_->size(); i++) {
      if ((*handle_vector_)[i].type == PlatformHandle::Type::MACH ||
          (*handle_vector_)[i].type == PlatformHandle::Type::MACH_NAME) {
        mach_port_t port = (*handle_vector_)[i].port;
        mach_ports_header_->entries[mach_port_index].index = i;
        mach_ports_header_->entries[mach_port_index].mach_port = port;
        mach_port_index++;
      }
    }
    mach_ports_header_->num_ports = static_cast<uint16_t>(mach_port_index);
  }
#endif
}

ScopedPlatformHandleVectorPtr Channel::Message::TakeHandles() {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (mach_ports_header_) {
    for (size_t i = 0; i < max_handles_; ++i) {
      mach_ports_header_->entries[i] =
          {0, static_cast<uint32_t>(MACH_PORT_NULL)};
    }
    mach_ports_header_->num_ports = 0;
  }
#endif
  if (is_legacy_message())
    legacy_header()->num_handles = 0;
  else
    header()->num_handles = 0;
  return std::move(handle_vector_);
}

ScopedPlatformHandleVectorPtr Channel::Message::TakeHandlesForTransport() {
#if defined(OS_WIN)
  // Not necessary on Windows.
  NOTREACHED();
  return nullptr;
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  if (handle_vector_) {
    for (auto it = handle_vector_->begin(); it != handle_vector_->end(); ) {
      if (it->type == PlatformHandle::Type::MACH ||
          it->type == PlatformHandle::Type::MACH_NAME) {
        // For Mach port names, we can can just leak them. They're not real
        // ports anyways. For real ports, they're leaked because this is a child
        // process and the remote process will take ownership.
        it = handle_vector_->erase(it);
      } else {
        ++it;
      }
    }
  }
  return std::move(handle_vector_);
#else
  return std::move(handle_vector_);
#endif
}

#if defined(OS_WIN)
// static
bool Channel::Message::RewriteHandles(base::ProcessHandle from_process,
                                      base::ProcessHandle to_process,
                                      PlatformHandleVector* handles) {
  bool success = true;
  for (size_t i = 0; i < handles->size(); ++i) {
    if (!(*handles)[i].is_valid()) {
      DLOG(ERROR) << "Refusing to duplicate invalid handle.";
      continue;
    }
    DCHECK_EQ((*handles)[i].owning_process, from_process);
    BOOL result = DuplicateHandle(
        from_process, (*handles)[i].handle, to_process,
        &(*handles)[i].handle, 0, FALSE,
        DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
    if (result) {
      (*handles)[i].owning_process = to_process;
    } else {
      success = false;

      // If handle duplication fails, the source handle will already be closed
      // due to DUPLICATE_CLOSE_SOURCE. Replace the handle in the message with
      // an invalid handle.
      (*handles)[i].handle = INVALID_HANDLE_VALUE;
      (*handles)[i].owning_process = base::GetCurrentProcessHandle();
    }
  }
  return success;
}
#endif

// Helper class for managing a Channel's read buffer allocations. This maintains
// a single contiguous buffer with the layout:
//
//   [discarded bytes][occupied bytes][unoccupied bytes]
//
// The Reserve() method ensures that a certain capacity of unoccupied bytes are
// available. It does not claim that capacity and only allocates new capacity
// when strictly necessary.
//
// Claim() marks unoccupied bytes as occupied.
//
// Discard() marks occupied bytes as discarded, signifying that their contents
// can be forgotten or overwritten.
//
// Realign() moves occupied bytes to the front of the buffer so that those
// occupied bytes are properly aligned.
//
// The most common Channel behavior in practice should result in very few
// allocations and copies, as memory is claimed and discarded shortly after
// being reserved, and future reservations will immediately reuse discarded
// memory.
class Channel::ReadBuffer {
 public:
  ReadBuffer() {
    size_ = kReadBufferSize;
    data_ = static_cast<char*>(base::AlignedAlloc(size_,
                                                  kChannelMessageAlignment));
  }

  ~ReadBuffer() {
    DCHECK(data_);
    base::AlignedFree(data_);
  }

  const char* occupied_bytes() const { return data_ + num_discarded_bytes_; }

  size_t num_occupied_bytes() const {
    return num_occupied_bytes_ - num_discarded_bytes_;
  }

  // Ensures the ReadBuffer has enough contiguous space allocated to hold
  // |num_bytes| more bytes; returns the address of the first available byte.
  char* Reserve(size_t num_bytes) {
    if (num_occupied_bytes_ + num_bytes > size_) {
      size_ = std::max(size_ * 2, num_occupied_bytes_ + num_bytes);
      void* new_data = base::AlignedAlloc(size_, kChannelMessageAlignment);
      memcpy(new_data, data_, num_occupied_bytes_);
      base::AlignedFree(data_);
      data_ = static_cast<char*>(new_data);
    }

    return data_ + num_occupied_bytes_;
  }

  // Marks the first |num_bytes| unoccupied bytes as occupied.
  void Claim(size_t num_bytes) {
    DCHECK_LE(num_occupied_bytes_ + num_bytes, size_);
    num_occupied_bytes_ += num_bytes;
  }

  // Marks the first |num_bytes| occupied bytes as discarded. This may result in
  // shrinkage of the internal buffer, and it is not safe to assume the result
  // of a previous Reserve() call is still valid after this.
  void Discard(size_t num_bytes) {
    DCHECK_LE(num_discarded_bytes_ + num_bytes, num_occupied_bytes_);
    num_discarded_bytes_ += num_bytes;

    if (num_discarded_bytes_ == num_occupied_bytes_) {
      // We can just reuse the buffer from the beginning in this common case.
      num_discarded_bytes_ = 0;
      num_occupied_bytes_ = 0;
    }

    if (num_discarded_bytes_ > kMaxUnusedReadBufferCapacity) {
      // In the uncommon case that we have a lot of discarded data at the
      // front of the buffer, simply move remaining data to a smaller buffer.
      size_t num_preserved_bytes = num_occupied_bytes_ - num_discarded_bytes_;
      size_ = std::max(num_preserved_bytes, kReadBufferSize);
      char* new_data = static_cast<char*>(
          base::AlignedAlloc(size_, kChannelMessageAlignment));
      memcpy(new_data, data_ + num_discarded_bytes_, num_preserved_bytes);
      base::AlignedFree(data_);
      data_ = new_data;
      num_discarded_bytes_ = 0;
      num_occupied_bytes_ = num_preserved_bytes;
    }

    if (num_occupied_bytes_ == 0 && size_ > kMaxUnusedReadBufferCapacity) {
      // Opportunistically shrink the read buffer back down to a small size if
      // it's grown very large. We only do this if there are no remaining
      // unconsumed bytes in the buffer to avoid copies in most the common
      // cases.
      size_ = kMaxUnusedReadBufferCapacity;
      base::AlignedFree(data_);
      data_ = static_cast<char*>(
          base::AlignedAlloc(size_, kChannelMessageAlignment));
    }
  }

  void Realign() {
    size_t num_bytes = num_occupied_bytes();
    memmove(data_, occupied_bytes(), num_bytes);
    num_discarded_bytes_ = 0;
    num_occupied_bytes_ = num_bytes;
  }

 private:
  char* data_ = nullptr;

  // The total size of the allocated buffer.
  size_t size_ = 0;

  // The number of discarded bytes at the beginning of the allocated buffer.
  size_t num_discarded_bytes_ = 0;

  // The total number of occupied bytes, including discarded bytes.
  size_t num_occupied_bytes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ReadBuffer);
};

Channel::Channel(Delegate* delegate)
    : delegate_(delegate), read_buffer_(new ReadBuffer) {
}

Channel::~Channel() {
}

void Channel::ShutDown() {
  delegate_ = nullptr;
  ShutDownImpl();
}

char* Channel::GetReadBuffer(size_t *buffer_capacity) {
  DCHECK(read_buffer_);
  size_t required_capacity = *buffer_capacity;
  if (!required_capacity)
    required_capacity = kReadBufferSize;

  *buffer_capacity = required_capacity;
  return read_buffer_->Reserve(required_capacity);
}

bool Channel::OnReadComplete(size_t bytes_read, size_t *next_read_size_hint) {
  bool did_dispatch_message = false;
  read_buffer_->Claim(bytes_read);
  while (read_buffer_->num_occupied_bytes() >= sizeof(Message::LegacyHeader)) {
    // Ensure the occupied data is properly aligned. If it isn't, a SIGBUS could
    // happen on architectures that don't allow misaligned words access (i.e.
    // anything other than x86). Only re-align when necessary to avoid copies.
    if (!IsAlignedForChannelMessage(
            reinterpret_cast<uintptr_t>(read_buffer_->occupied_bytes()))) {
      read_buffer_->Realign();
    }

    // We have at least enough data available for a LegacyHeader.
    const Message::LegacyHeader* legacy_header =
        reinterpret_cast<const Message::LegacyHeader*>(
            read_buffer_->occupied_bytes());

    if (legacy_header->num_bytes < sizeof(Message::LegacyHeader) ||
        legacy_header->num_bytes > kMaxChannelMessageSize) {
      LOG(ERROR) << "Invalid message size: " << legacy_header->num_bytes;
      return false;
    }

    if (read_buffer_->num_occupied_bytes() < legacy_header->num_bytes) {
      // Not enough data available to read the full message. Hint to the
      // implementation that it should try reading the full size of the message.
      *next_read_size_hint =
          legacy_header->num_bytes - read_buffer_->num_occupied_bytes();
      return true;
    }

    const Message::Header* header = nullptr;
    if (legacy_header->message_type != Message::MessageType::NORMAL_LEGACY) {
      header = reinterpret_cast<const Message::Header*>(legacy_header);
    }

    size_t extra_header_size = 0;
    const void* extra_header = nullptr;
    size_t payload_size = 0;
    void* payload = nullptr;
    if (header) {
      if (header->num_header_bytes < sizeof(Message::Header) ||
          header->num_header_bytes > header->num_bytes) {
        LOG(ERROR) << "Invalid message header size: "
                   << header->num_header_bytes;
        return false;
      }
      extra_header_size = header->num_header_bytes - sizeof(Message::Header);
      extra_header = extra_header_size ? header + 1 : nullptr;
      payload_size = header->num_bytes - header->num_header_bytes;
      payload = payload_size
                    ? reinterpret_cast<Message::Header*>(
                          const_cast<char*>(read_buffer_->occupied_bytes()) +
                          header->num_header_bytes)
                    : nullptr;
    } else {
      payload_size = legacy_header->num_bytes - sizeof(Message::LegacyHeader);
      payload = payload_size
                    ? const_cast<Message::LegacyHeader*>(&legacy_header[1])
                    : nullptr;
    }

    const uint16_t num_handles =
        header ? header->num_handles : legacy_header->num_handles;
    ScopedPlatformHandleVectorPtr handles;
    if (num_handles > 0) {
      if (!GetReadPlatformHandles(num_handles, extra_header, extra_header_size,
                                  &handles)) {
        return false;
      }

      if (!handles) {
        // Not enough handles available for this message.
        break;
      }
    }

    // We've got a complete message! Dispatch it and try another.
    if (legacy_header->message_type != Message::MessageType::NORMAL_LEGACY &&
        legacy_header->message_type != Message::MessageType::NORMAL) {
      if (!OnControlMessage(legacy_header->message_type, payload, payload_size,
                            std::move(handles))) {
        return false;
      }
      did_dispatch_message = true;
    } else if (delegate_) {
      delegate_->OnChannelMessage(payload, payload_size, std::move(handles));
      did_dispatch_message = true;
    }

    read_buffer_->Discard(legacy_header->num_bytes);
  }

  *next_read_size_hint = did_dispatch_message ? 0 : kReadBufferSize;
  return true;
}

void Channel::OnError() {
  if (delegate_)
    delegate_->OnChannelError();
}

bool Channel::OnControlMessage(Message::MessageType message_type,
                               const void* payload,
                               size_t payload_size,
                               ScopedPlatformHandleVectorPtr handles) {
  return false;
}

}  // namespace edk
}  // namespace mojo
