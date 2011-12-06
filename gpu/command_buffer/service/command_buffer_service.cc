// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/command_buffer_service.h"

#include <limits>

#include "base/process_util.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"

using ::base::SharedMemory;

namespace gpu {

CommandBufferService::CommandBufferService()
    : num_entries_(0),
      get_offset_(0),
      put_offset_(0),
      token_(0),
      generation_(0),
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
  state.context_lost_reason = context_lost_reason_;
  state.generation = ++generation_;

  return state;
}

CommandBufferService::State CommandBufferService::GetLastState() {
  return GetState();
}

CommandBufferService::State CommandBufferService::FlushSync(
    int32 put_offset, int32 last_known_get) {
  if (put_offset < 0 || put_offset > num_entries_) {
    error_ = gpu::error::kOutOfBounds;
    return GetState();
  }

  put_offset_ = put_offset;

  if (!put_offset_change_callback_.is_null())
    put_offset_change_callback_.Run();

  return GetState();
}

void CommandBufferService::Flush(int32 put_offset) {
  if (put_offset < 0 || put_offset > num_entries_) {
    error_ = gpu::error::kOutOfBounds;
    return;
  }

  put_offset_ = put_offset;

  if (!put_offset_change_callback_.is_null())
    put_offset_change_callback_.Run();
}

void CommandBufferService::SetGetOffset(int32 get_offset) {
  DCHECK(get_offset >= 0 && get_offset < num_entries_);
  get_offset_ = get_offset;
}

int32 CommandBufferService::CreateTransferBuffer(size_t size,
                                                 int32 id_request) {
  SharedMemory buffer;
  if (!buffer.CreateAnonymous(size))
    return -1;

  return RegisterTransferBuffer(&buffer, size, id_request);
}

int32 CommandBufferService::RegisterTransferBuffer(
    base::SharedMemory* shared_memory, size_t size, int32 id_request) {
  // Check we haven't exceeded the range that fits in a 32-bit integer.
  if (unused_registered_object_elements_.empty()) {
    if (registered_objects_.size() > std::numeric_limits<uint32>::max())
      return -1;
  }

  // Check that the requested ID is sane (not too large, or less than -1)
  if (id_request != -1 && (id_request > 100 || id_request < -1))
    return -1;

  // Duplicate the handle.
  base::SharedMemoryHandle duped_shared_memory_handle;
  if (!shared_memory->ShareToProcess(base::GetCurrentProcessHandle(),
                                     &duped_shared_memory_handle)) {
    return -1;
  }
  scoped_ptr<SharedMemory> duped_shared_memory(
      new SharedMemory(duped_shared_memory_handle, false));

  // Map the shared memory into this process. This validates the size.
  if (!duped_shared_memory->Map(size))
    return -1;

  // If it could be mapped, allocate an ID and register the shared memory with
  // that ID.
  Buffer buffer;
  buffer.ptr = duped_shared_memory->memory();
  buffer.size = size;
  buffer.shared_memory = duped_shared_memory.release();

  // If caller requested specific id, first try to use id_request.
  if (id_request != -1) {
    int32 cur_size = static_cast<int32>(registered_objects_.size());
    if (cur_size <= id_request) {
      // Pad registered_objects_ to reach id_request.
      registered_objects_.resize(static_cast<size_t>(id_request + 1));
      for (int32 id = cur_size; id < id_request; ++id)
        unused_registered_object_elements_.insert(id);
      registered_objects_[id_request] = buffer;
      return id_request;
    } else if (!registered_objects_[id_request].shared_memory) {
      // id_request is already in free list.
      registered_objects_[id_request] = buffer;
      unused_registered_object_elements_.erase(id_request);
      return id_request;
    }
  }

  if (unused_registered_object_elements_.empty()) {
    int32 handle = static_cast<int32>(registered_objects_.size());
    registered_objects_.push_back(buffer);
    return handle;
  } else {
    int32 handle = *unused_registered_object_elements_.begin();
    unused_registered_object_elements_.erase(
        unused_registered_object_elements_.begin());
    DCHECK(!registered_objects_[handle].shared_memory);
    registered_objects_[handle] = buffer;
    return handle;
  }
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

  return registered_objects_[handle];
}

void CommandBufferService::SetToken(int32 token) {
  token_ = token;
}

void CommandBufferService::SetParseError(error::Error error) {
  if (error_ == error::kNoError) {
    error_ = error;
    if (!parse_error_callback_.is_null())
      parse_error_callback_.Run();
  }
}

void CommandBufferService::SetContextLostReason(
    error::ContextLostReason reason) {
  context_lost_reason_ = reason;
}

void CommandBufferService::SetPutOffsetChangeCallback(
    const base::Closure& callback) {
  put_offset_change_callback_ = callback;
}

void CommandBufferService::SetParseErrorCallback(
    const base::Closure& callback) {
  parse_error_callback_ = callback;
}

}  // namespace gpu
