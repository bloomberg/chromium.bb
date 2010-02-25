// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_BUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_BUFFER_MANAGER_H_

#include <map>
#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "gpu/command_buffer/service/gl_utils.h"

namespace gpu {
namespace gles2 {

// This class keeps track of the buffers and their sizes so we can do
// bounds checking.
//
// NOTE: To support shared resources an instance of this class will need to be
// shared by multiple GLES2Decoders.
class BufferManager {
 public:
  // Info about Buffers currently in the system.
  class BufferInfo : public base::RefCounted<BufferInfo> {
   public:
    typedef scoped_refptr<BufferInfo> Ref;

    explicit BufferInfo(GLuint buffer_id)
        : buffer_id_(buffer_id),
          size_(0) {
    }

    GLuint buffer_id() const {
      return buffer_id_;
    }

    GLsizeiptr size() const {
      return size_;
    }

    void set_size(GLsizeiptr size) {
      size_ = size;
    }

    // Returns the maximum value in the buffer for the given range
    // interpreted as the given type.
    GLuint GetMaxValueForRange(GLuint offset, GLsizei count, GLenum type);

    bool IsDeleted() {
      return buffer_id_ == 0;
    }

   private:
    friend class BufferManager;
    friend class base::RefCounted<BufferInfo>;

    ~BufferInfo() { }

    void MarkAsDeleted() {
      buffer_id_ = 0;
    }

    GLuint buffer_id_;
    GLsizeiptr size_;
  };

  BufferManager() { }

  // Creates a BufferInfo for the given buffer.
  void CreateBufferInfo(GLuint buffer_id);

  // Gets the buffer info for the given buffer.
  BufferInfo* GetBufferInfo(GLuint buffer_id);

  // Removes a buffer info for the given buffer.
  void RemoveBufferInfo(GLuint buffer_id);

 private:
  // Info for each buffer in the system.
  // TODO(gman): Choose a faster container.
  typedef std::map<GLuint, BufferInfo::Ref> BufferInfoMap;
  BufferInfoMap buffer_infos_;

  DISALLOW_COPY_AND_ASSIGN(BufferManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_BUFFER_MANAGER_H_


