// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/transfer_buffer_manager.h"

#include <limits>

#include "base/process_util.h"
#include "base/debug/trace_event.h"

using ::base::SharedMemory;

namespace gpu {

TransferBufferManagerInterface::~TransferBufferManagerInterface() {
}

TransferBufferManager::TransferBufferManager()
    : shared_memory_bytes_allocated_(0) {
  // Element zero is always NULL.
  registered_objects_.push_back(Buffer());
}

TransferBufferManager::~TransferBufferManager() {
  for (size_t i = 0; i < registered_objects_.size(); ++i) {
    if (registered_objects_[i].shared_memory) {
      shared_memory_bytes_allocated_ -= registered_objects_[i].size;
      delete registered_objects_[i].shared_memory;
    }
  }
  // TODO(gman): Should we report 0 bytes to TRACE here?
}

bool TransferBufferManager::Initialize() {
  return true;
}

int32 TransferBufferManager::CreateTransferBuffer(
    size_t size, int32 id_request) {
  SharedMemory buffer;
  if (!buffer.CreateAnonymous(size))
    return -1;

  shared_memory_bytes_allocated_ += size;
  TRACE_COUNTER_ID1(
      "CommandBuffer", "SharedMemory", this, shared_memory_bytes_allocated_);

  return RegisterTransferBuffer(&buffer, size, id_request);
}

int32 TransferBufferManager::RegisterTransferBuffer(
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

void TransferBufferManager::DestroyTransferBuffer(int32 handle) {
  if (handle <= 0)
    return;

  if (static_cast<size_t>(handle) >= registered_objects_.size())
    return;

  shared_memory_bytes_allocated_ -= registered_objects_[handle].size;
  TRACE_COUNTER_ID1(
      "CommandBuffer", "SharedMemory", this, shared_memory_bytes_allocated_);

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

Buffer TransferBufferManager::GetTransferBuffer(int32 handle) {
  if (handle < 0)
    return Buffer();

  if (static_cast<size_t>(handle) >= registered_objects_.size())
    return Buffer();

  return registered_objects_[handle];
}

}  // namespace gpu

