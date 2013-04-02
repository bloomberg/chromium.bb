// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_BUFFER_TRACKER_H_
#define GPU_COMMAND_BUFFER_CLIENT_BUFFER_TRACKER_H_

#include <GLES2/gl2.h>

#include <queue>
#include "../client/hash_tables.h"
#include "../common/gles2_cmd_format.h"
#include "gles2_impl_export.h"

namespace gpu {

class CommandBufferHelper;
class MappedMemoryManager;

namespace gles2 {

// Tracks buffer objects for client side of command buffer.
class GLES2_IMPL_EXPORT BufferTracker {
 public:
  class GLES2_IMPL_EXPORT Buffer {
   public:
    Buffer(GLuint id,
           unsigned int size,
           int32 shm_id,
           uint32 shm_offset,
           void* address)
        : id_(id),
          size_(size),
          shm_id_(shm_id),
          shm_offset_(shm_offset),
          address_(address),
          mapped_(false),
          transfer_ready_token_(0) {
    }

    GLenum id() const {
      return id_;
    }

    unsigned int size() const {
      return size_;
    }

    int32 shm_id() const {
      return shm_id_;
    }

    uint32 shm_offset() const {
      return shm_offset_;
    }

    void* address() const {
      return address_;
    }

    void set_mapped(bool mapped) {
      mapped_ = mapped;
    }

    bool mapped() const {
      return mapped_;
    }

    void set_transfer_ready_token(int token) {
      transfer_ready_token_ = token;
    }

    uint32 transfer_ready_token() const {
      return transfer_ready_token_;
    }

   private:
    friend class BufferTracker;
    friend class BufferTrackerTest;

    GLuint id_;
    unsigned int size_;
    int32 shm_id_;
    uint32 shm_offset_;
    void* address_;
    bool mapped_;
    int32 transfer_ready_token_;
  };

  BufferTracker(MappedMemoryManager* manager);
  ~BufferTracker();

  Buffer* CreateBuffer(GLuint id, GLsizeiptr size);
  Buffer* GetBuffer(GLuint id);
  void RemoveBuffer(GLuint id);

  // Frees the block of memory associated with buffer, pending the passage
  // of a token.
  void FreePendingToken(Buffer*, int32 token);

 private:
  typedef gpu::hash_map<GLuint, Buffer*> BufferMap;

  MappedMemoryManager* mapped_memory_;
  BufferMap buffers_;

  DISALLOW_COPY_AND_ASSIGN(BufferTracker);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_BUFFER_TRACKER_H_
