// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel.h"

#include <string.h>

#include <algorithm>
#include <limits>

#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "mojo/edk/embedder/platform_handle.h"

namespace mojo {
namespace edk {

namespace {

static_assert(sizeof(Channel::Message::Header) % kChannelMessageAlignment == 0,
    "Invalid Header size.");

}  // namespace

const size_t kReadBufferSize = 4096;
const size_t kMaxUnusedReadBufferCapacity = 256 * 1024;
const size_t kMaxChannelMessageSize = 256 * 1024 * 1024;

Channel::Message::Message(size_t payload_size, size_t num_handles) {
  size_ = payload_size + sizeof(Header);
#if defined(OS_WIN)
  // On Windows we serialize platform handles directly into the message buffer.
  size_ += num_handles * sizeof(PlatformHandle);
#endif

  data_ = static_cast<char*>(base::AlignedAlloc(size_,
                                                kChannelMessageAlignment));
  header_ = reinterpret_cast<Header*>(data_);

  DCHECK_LE(size_, std::numeric_limits<uint32_t>::max());
  header_->num_bytes = static_cast<uint32_t>(size_);

  DCHECK_LE(num_handles, std::numeric_limits<uint16_t>::max());
  header_->num_handles = static_cast<uint16_t>(num_handles);

  header_->padding = 0;

#if defined(OS_WIN)
  if (num_handles > 0) {
    handles_ = reinterpret_cast<PlatformHandle*>(
        data_ + sizeof(Header) + payload_size);
    // Initialize all handles to invalid values.
    for (size_t i = 0; i < header_->num_handles; ++i)
      handles()[i] = PlatformHandle();
  }
#endif
}

Channel::Message::~Message() {
#if defined(OS_WIN)
  // On POSIX the ScopedPlatformHandleVectorPtr will do this for us.
  for (size_t i = 0; i < header_->num_handles; ++i)
    handles()[i].CloseIfNecessary();
#endif
  base::AlignedFree(data_);
}

// static
Channel::MessagePtr Channel::Message::Deserialize(const void* data,
                                                  size_t data_num_bytes) {
#if !defined(OS_WIN)
  // We only serialize messages into other messages when performing message
  // relay on Windows.
  NOTREACHED();
#endif
  if (data_num_bytes < sizeof(Header))
    return nullptr;

  const Header* header = reinterpret_cast<const Header*>(data);
  if (header->num_bytes != data_num_bytes) {
    DLOG(ERROR) << "Decoding invalid message: " << header->num_bytes
                << " != " << data_num_bytes;
    return nullptr;
  }

  uint32_t handles_size = header->num_handles * sizeof(PlatformHandle);
  if (data_num_bytes < sizeof(Header) + handles_size) {
    DLOG(ERROR) << "Decoding invalid message:" << data_num_bytes
                << " < " << (sizeof(Header) + handles_size);
    return nullptr;
  }

  DCHECK_LE(handles_size, data_num_bytes - sizeof(Header));

  MessagePtr message(new Message(data_num_bytes - sizeof(Header) - handles_size,
                                 header->num_handles));

  DCHECK_EQ(message->data_num_bytes(), data_num_bytes);

  // Copy all bytes, including the serialized handles.
  memcpy(message->mutable_payload(),
         static_cast<const char*>(data) + sizeof(Header),
         data_num_bytes - sizeof(Header));

  return message;
}

size_t Channel::Message::payload_size() const {
#if defined(OS_WIN)
  return size_ - sizeof(Header) -
      sizeof(PlatformHandle) * header_->num_handles;
#else
  return header_->num_bytes - sizeof(Header);
#endif
}

PlatformHandle* Channel::Message::handles() {
  if (header_->num_handles == 0)
    return nullptr;
#if defined(OS_WIN)
  return reinterpret_cast<PlatformHandle*>(static_cast<char*>(data_) +
                                           sizeof(Header) + payload_size());
#else
  CHECK(handle_vector_);
  return handle_vector_->data();
#endif
}

void Channel::Message::SetHandles(ScopedPlatformHandleVectorPtr new_handles) {
  if (header_->num_handles == 0) {
    CHECK(!new_handles || new_handles->size() == 0);
    return;
  }

  CHECK(new_handles && new_handles->size() == header_->num_handles);
#if defined(OS_WIN)
  memcpy(handles(), new_handles->data(),
         sizeof(PlatformHandle) * header_->num_handles);
  new_handles->clear();
#else
  std::swap(handle_vector_, new_handles);
#endif
}

ScopedPlatformHandleVectorPtr Channel::Message::TakeHandles() {
#if defined(OS_WIN)
  if (header_->num_handles == 0)
    return ScopedPlatformHandleVectorPtr();
  ScopedPlatformHandleVectorPtr moved_handles(
      new PlatformHandleVector(header_->num_handles));
  for (size_t i = 0; i < header_->num_handles; ++i)
    std::swap(moved_handles->at(i), handles()[i]);
  return moved_handles;
#else
  return std::move(handle_vector_);
#endif
}

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

    // TODO: we should also adaptively shrink the buffer in case of the
    // occasional abnormally large read.
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
  while (read_buffer_->num_occupied_bytes() >= sizeof(Message::Header)) {
    // We have at least enough data available for a MessageHeader.
    const Message::Header* header = reinterpret_cast<const Message::Header*>(
        read_buffer_->occupied_bytes());
    if (header->num_bytes < sizeof(Message::Header) ||
        header->num_bytes > kMaxChannelMessageSize) {
      LOG(ERROR) << "Invalid message size: " << header->num_bytes;
      return false;
    }

    if (read_buffer_->num_occupied_bytes() < header->num_bytes) {
      // Not enough data available to read the full message. Hint to the
      // implementation that it should try reading the full size of the message.
      *next_read_size_hint =
          header->num_bytes - read_buffer_->num_occupied_bytes();
      return true;
    }

    size_t payload_size = header->num_bytes - sizeof(Message::Header);
    void* payload = payload_size ? const_cast<Message::Header*>(&header[1])
                                 : nullptr;

    ScopedPlatformHandleVectorPtr handles;
    if (header->num_handles > 0) {
      handles = GetReadPlatformHandles(header->num_handles,
                                       &payload, &payload_size);
      if (!handles) {
        // Not enough handles available for this message.
        break;
      }
    }

    // We've got a complete message! Dispatch it and try another.
    if (delegate_) {
      delegate_->OnChannelMessage(payload, payload_size, std::move(handles));
      did_dispatch_message = true;
    }

    read_buffer_->Discard(header->num_bytes);
  }

  *next_read_size_hint = did_dispatch_message ? 0 : kReadBufferSize;
  return true;
}

void Channel::OnError() {
  if (delegate_)
    delegate_->OnChannelError();
}

}  // namespace edk
}  // namespace mojo
