// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../client/buffer_tracker.h"

#include "../client/atomicops.h"
#include "../client/cmd_buffer_helper.h"
#include "../client/mapped_memory.h"

namespace gpu {
namespace gles2 {

BufferTracker::BufferTracker(MappedMemoryManager* manager)
    : mapped_memory_(manager) {
}

BufferTracker::~BufferTracker() {
  while (!buffers_.empty()) {
    RemoveBuffer(buffers_.begin()->first);
  }
}

BufferTracker::Buffer* BufferTracker::CreateBuffer(
    GLuint id, GLsizeiptr size) {
  GPU_DCHECK_NE(0u, id);
  GPU_DCHECK_LE(0, size);
  int32 shm_id = -1;
  uint32 shm_offset = 0;
  void* address = NULL;
  if (size) {
    address = mapped_memory_->Alloc(size, &shm_id, &shm_offset);
    if (!address) {
      return NULL;
    }
  }

  Buffer* buffer = new Buffer(id, size, shm_id, shm_offset, address);
  std::pair<BufferMap::iterator, bool> result =
      buffers_.insert(std::make_pair(id, buffer));
  GPU_DCHECK(result.second);
  return buffer;
}

BufferTracker::Buffer* BufferTracker::GetBuffer(GLuint client_id) {
  BufferMap::iterator it = buffers_.find(client_id);
  return it != buffers_.end() ? it->second : NULL;
}

void BufferTracker::RemoveBuffer(GLuint client_id) {
  BufferMap::iterator it = buffers_.find(client_id);
  if (it != buffers_.end()) {
    Buffer* buffer = it->second;
    buffers_.erase(it);
    if (buffer->address_)
      mapped_memory_->Free(buffer->address_);
    delete buffer;
  }
}

void BufferTracker::FreePendingToken(Buffer* buffer, int32 token) {
  if (buffer->address_)
    mapped_memory_->FreePendingToken(buffer->address_, token);
  buffer->size_ = 0;
  buffer->shm_id_ = 0;
  buffer->shm_offset_ = 0;
  buffer->address_ = NULL;
}


}  // namespace gles2
}  // namespace gpu
