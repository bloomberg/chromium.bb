// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/transfer_buffer_manager.h"

#include <limits>

#include "base/process_util.h"
#include "base/debug/trace_event.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"

using ::base::SharedMemory;

namespace gpu {

TransferBufferManagerInterface::~TransferBufferManagerInterface() {
}

TransferBufferManager::TransferBufferManager()
    : shared_memory_bytes_allocated_(0) {
}

TransferBufferManager::~TransferBufferManager() {
  while (!registered_buffers_.empty()) {
    BufferMap::iterator it = registered_buffers_.begin();
    DCHECK(shared_memory_bytes_allocated_ >= it->second.size);
    shared_memory_bytes_allocated_ -= it->second.size;
    delete it->second.shared_memory;
    registered_buffers_.erase(it);
  }
  DCHECK(!shared_memory_bytes_allocated_);
}

bool TransferBufferManager::Initialize() {
  return true;
}

bool TransferBufferManager::RegisterTransferBuffer(
    int32 id,
    base::SharedMemory* shared_memory,
    size_t size) {
  if (id <= 0) {
    DVLOG(0) << "Cannot register transfer buffer with non-positive ID.";
    return false;
  }

  // Fail if the ID is in use.
  if (registered_buffers_.find(id) != registered_buffers_.end()) {
    DVLOG(0) << "Buffer ID already in use.";
    return false;
  }

  // Duplicate the handle.
  base::SharedMemoryHandle duped_shared_memory_handle;
  if (!shared_memory->ShareToProcess(base::GetCurrentProcessHandle(),
                                     &duped_shared_memory_handle)) {
    DVLOG(0) << "Failed to duplicate shared memory handle.";
    return false;
  }
  scoped_ptr<SharedMemory> duped_shared_memory(
      new SharedMemory(duped_shared_memory_handle, false));

  // Map the shared memory into this process. This validates the size.
  if (!duped_shared_memory->Map(size)) {
    DVLOG(0) << "Failed to map shared memory.";
    return false;
  }

  // If it could be mapped register the shared memory with the ID.
  Buffer buffer;
  buffer.ptr = duped_shared_memory->memory();
  buffer.size = size;
  buffer.shared_memory = duped_shared_memory.release();

  shared_memory_bytes_allocated_ += size;
  TRACE_COUNTER_ID1(
      "gpu", "GpuTransferBufferMemory", this, shared_memory_bytes_allocated_);

  registered_buffers_[id] = buffer;

  return true;
}

void TransferBufferManager::DestroyTransferBuffer(int32 id) {
  BufferMap::iterator it = registered_buffers_.find(id);
  if (it == registered_buffers_.end()) {
    DVLOG(0) << "Transfer buffer ID was not registered.";
    return;
  }

  DCHECK(shared_memory_bytes_allocated_ >= it->second.size);
  shared_memory_bytes_allocated_ -= it->second.size;
  TRACE_COUNTER_ID1(
      "CommandBuffer", "SharedMemory", this, shared_memory_bytes_allocated_);

  delete it->second.shared_memory;
  registered_buffers_.erase(it);
}

Buffer TransferBufferManager::GetTransferBuffer(int32 id) {
  if (id == 0)
    return Buffer();

  BufferMap::iterator it = registered_buffers_.find(id);
  if (it == registered_buffers_.end())
    return Buffer();

  return it->second;
}

}  // namespace gpu

