// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_buffer_service.h"

#include <limits>

#include "base/callback.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"

using ::base::SharedMemory;

namespace gpu {

CommandBufferService::CommandBufferService()
    : num_entries_(0),
      get_offset_(0),
      put_offset_(0),
      token_(0),
      error_(error::kNoError) {
  // Element zero is always NULL.
  registered_objects_.push_back(linked_ptr<SharedMemory>());
}

CommandBufferService::~CommandBufferService() {
}

bool CommandBufferService::Initialize(int32 size) {
  // Fail if already initialized.
  if (ring_buffer_.get()) {
    LOG(ERROR) << "CommandBufferService::Initialize "
               << "failed because already initialized.";
    return false;
  }

  if (size <= 0 || size > kMaxCommandBufferSize) {
    LOG(ERROR) << "CommandBufferService::Initialize "
               << "because command buffer size was invalid.";
    return false;
  }

  num_entries_ = size / sizeof(CommandBufferEntry);

  ring_buffer_.reset(new SharedMemory);
  if (ring_buffer_->Create(std::wstring(), false, false, size)) {
    if (ring_buffer_->Map(size))
      return true;
  }

  num_entries_ = 0;
  ring_buffer_.reset();

  LOG(ERROR) << "CommandBufferService::Initialize failed because ring buffer "
             << "could not be created or mapped ";

  return false;
}

Buffer CommandBufferService::GetRingBuffer() {
  Buffer buffer;
  if (ring_buffer_.get()) {
    buffer.ptr = ring_buffer_->memory();
    buffer.size = ring_buffer_->max_size();
    buffer.shared_memory = ring_buffer_.get();
  }
  return buffer;
}

CommandBufferService::State CommandBufferService::GetState() {
  State state;
  state.num_entries = num_entries_;
  state.get_offset = get_offset_;
  state.put_offset = put_offset_;
  state.token = token_;
  state.error = error_;

  return state;
}

CommandBufferService::State CommandBufferService::Flush(int32 put_offset) {
  if (put_offset < 0 || put_offset > num_entries_) {
    error_ = gpu::error::kOutOfBounds;
    return GetState();
  }

  put_offset_ = put_offset;

  if (put_offset_change_callback_.get()) {
    put_offset_change_callback_->Run();
  }

  return GetState();
}

void CommandBufferService::SetGetOffset(int32 get_offset) {
  DCHECK(get_offset >= 0 && get_offset < num_entries_);
  get_offset_ = get_offset;
}

int32 CommandBufferService::CreateTransferBuffer(size_t size) {
  linked_ptr<SharedMemory> buffer(new SharedMemory);
  if (!buffer->Create(std::wstring(), false, false, size))
    return -1;

  if (unused_registered_object_elements_.empty()) {
    // Check we haven't exceeded the range that fits in a 32-bit integer.
    if (registered_objects_.size() > std::numeric_limits<uint32>::max())
      return -1;

    int32 handle = static_cast<int32>(registered_objects_.size());
    registered_objects_.push_back(buffer);
    return handle;
  }

  int32 handle = *unused_registered_object_elements_.begin();
  unused_registered_object_elements_.erase(
      unused_registered_object_elements_.begin());
  DCHECK(!registered_objects_[handle].get());
  registered_objects_[handle] = buffer;
  return handle;
}

void CommandBufferService::DestroyTransferBuffer(int32 handle) {
  if (handle <= 0)
    return;

  if (static_cast<size_t>(handle) >= registered_objects_.size())
    return;

  registered_objects_[handle].reset();
  unused_registered_object_elements_.insert(handle);

  // Remove all null objects from the end of the vector. This allows the vector
  // to shrink when, for example, all objects are unregistered. Note that this
  // loop never removes element zero, which is always NULL.
  while (registered_objects_.size() > 1 && !registered_objects_.back().get()) {
    registered_objects_.pop_back();
    unused_registered_object_elements_.erase(
        static_cast<int32>(registered_objects_.size()));
  }
}

Buffer CommandBufferService::GetTransferBuffer(int32 handle) {
  if (handle < 0)
    return Buffer();

  if (static_cast<size_t>(handle) >= registered_objects_.size())
    return Buffer();

  base::SharedMemory* shared_memory = registered_objects_[handle].get();
  if (!shared_memory)
    return Buffer();

  if (!shared_memory->memory()) {
    if (!shared_memory->Map(shared_memory->max_size()))
      return Buffer();
  }

  Buffer buffer;
  buffer.ptr = shared_memory->memory();
  buffer.size = shared_memory->max_size();
  buffer.shared_memory = shared_memory;
  return buffer;
}

void CommandBufferService::SetToken(int32 token) {
  token_ = token;
}

void CommandBufferService::SetParseError(error::Error error) {
  if (error_ == error::kNoError) {
    error_ = error;
  }
}

void CommandBufferService::SetPutOffsetChangeCallback(
    Callback0::Type* callback) {
  put_offset_change_callback_.reset(callback);
}

}  // namespace gpu
