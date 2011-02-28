// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_buffer_service.h"

#include <limits>

#include "base/callback.h"
#include "base/process_util.h"
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
  registered_objects_.push_back(Buffer());
}

CommandBufferService::~CommandBufferService() {
  delete ring_buffer_.shared_memory;

  for (size_t i = 0; i < registered_objects_.size(); ++i) {
    if (registered_objects_[i].shared_memory)
      delete registered_objects_[i].shared_memory;
  }
}

bool CommandBufferService::Initialize(int32 size) {
  // Fail if already initialized.
  if (ring_buffer_.shared_memory) {
    LOG(ERROR) << "Failed because already initialized.";
    return false;
  }

  if (size <= 0 || size > kMaxCommandBufferSize) {
    LOG(ERROR) << "Failed because command buffer size was invalid.";
    return false;
  }

  num_entries_ = size / sizeof(CommandBufferEntry);

  SharedMemory shared_memory;
  if (!shared_memory.CreateAnonymous(size)) {
    LOG(ERROR) << "Failed to create shared memory for command buffer.";
    return true;
  }

  return Initialize(&shared_memory, size);
}

bool CommandBufferService::Initialize(base::SharedMemory* buffer, int32 size) {
  // Fail if already initialized.
  if (ring_buffer_.shared_memory) {
    LOG(ERROR) << "Failed because already initialized.";
    return false;
  }

  base::SharedMemoryHandle shared_memory_handle;
  if (!buffer->ShareToProcess(base::GetCurrentProcessHandle(),
                              &shared_memory_handle)) {
    LOG(ERROR) << "Failed to duplicate command buffer shared memory handle.";
    return false;
  }

  ring_buffer_.shared_memory = new base::SharedMemory(shared_memory_handle,
                                                      false);
  if (!ring_buffer_.shared_memory->Map(size)) {
    LOG(ERROR) << "Failed because ring buffer could not be created or mapped ";
    delete ring_buffer_.shared_memory;
    ring_buffer_.shared_memory = NULL;
    return false;
  }

  ring_buffer_.ptr = ring_buffer_.shared_memory->memory();
  ring_buffer_.size = size;
  num_entries_ = size / sizeof(CommandBufferEntry);

  return true;
}

Buffer CommandBufferService::GetRingBuffer() {
  return ring_buffer_;
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

CommandBufferService::State CommandBufferService::FlushSync(int32 put_offset) {
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

void CommandBufferService::Flush(int32 put_offset) {
  FlushSync(put_offset);
}

void CommandBufferService::SetGetOffset(int32 get_offset) {
  DCHECK(get_offset >= 0 && get_offset < num_entries_);
  get_offset_ = get_offset;
}

int32 CommandBufferService::CreateTransferBuffer(size_t size) {
  SharedMemory buffer;
  if (!buffer.CreateAnonymous(size))
    return -1;

  return RegisterTransferBuffer(&buffer, size);
}

int32 CommandBufferService::RegisterTransferBuffer(
    base::SharedMemory* shared_memory, size_t size) {
  // Duplicate the handle.
  base::SharedMemoryHandle shared_memory_handle;
  if (!shared_memory->ShareToProcess(base::GetCurrentProcessHandle(),
                                     &shared_memory_handle)) {
    return -1;
  }

  Buffer buffer;
  buffer.ptr = NULL;
  buffer.size = size;
  buffer.shared_memory = new SharedMemory(shared_memory_handle, false);

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
  DCHECK(!registered_objects_[handle].shared_memory);
  registered_objects_[handle] = buffer;
  return handle;
}

void CommandBufferService::DestroyTransferBuffer(int32 handle) {
  if (handle <= 0)
    return;

  if (static_cast<size_t>(handle) >= registered_objects_.size())
    return;

  delete registered_objects_[handle].shared_memory;
  registered_objects_[handle] = Buffer();
  unused_registered_object_elements_.insert(handle);

  // Remove all null objects from the end of the vector. This allows the vector
  // to shrink when, for example, all objects are unregistered. Note that this
  // loop never removes element zero, which is always NULL.
  while (registered_objects_.size() > 1 &&
      !registered_objects_.back().shared_memory) {
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

   Buffer buffer = registered_objects_[handle];
   if (!buffer.shared_memory)
     return Buffer();

  if (!buffer.shared_memory->memory()) {
    if (!buffer.shared_memory->Map(buffer.size))
      return Buffer();
  }

  buffer.ptr = buffer.shared_memory->memory();
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
