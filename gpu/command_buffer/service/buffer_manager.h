// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_BUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_BUFFER_MANAGER_H_

#include <map>
#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {

// This class keeps track of the buffers and their sizes so we can do
// bounds checking.
//
// NOTE: To support shared resources an instance of this class will need to be
// shared by multiple GLES2Decoders.
class GPU_EXPORT BufferManager {
 public:
  // Info about Buffers currently in the system.
  class GPU_EXPORT Buffer : public base::RefCounted<Buffer> {
   public:
    Buffer(BufferManager* manager, GLuint service_id);

    GLuint service_id() const {
      return service_id_;
    }

    GLsizeiptr size() const {
      return size_;
    }

    GLenum usage() const {
      return usage_;
    }

    // Sets a range of data for this buffer. Returns false if the offset or size
    // is out of range.
    bool SetRange(
      GLintptr offset, GLsizeiptr size, const GLvoid * data);

    // Gets the maximum value in the buffer for the given range interpreted as
    // the given type. Returns false if offset and count are out of range.
    // offset is in bytes.
    // count is in elements of type.
    bool GetMaxValueForRange(GLuint offset, GLsizei count, GLenum type,
                             GLuint* max_value);

    // Returns a pointer to shadowed data.
    const void* GetRange(GLintptr offset, GLsizeiptr size) const;

    bool IsDeleted() const {
      return deleted_;
    }

    bool IsValid() const {
      return target() && !IsDeleted();
    }

   private:
    friend class BufferManager;
    friend class BufferManagerTestBase;
    friend class base::RefCounted<Buffer>;

    // Represents a range in a buffer.
    class Range {
     public:
      Range(GLuint offset, GLsizei count, GLenum type)
          : offset_(offset),
            count_(count),
            type_(type) {
      }

      // A less functor provided for std::map so it can find ranges.
      struct Less {
        bool operator() (const Range& lhs, const Range& rhs) const {
          if (lhs.offset_ != rhs.offset_) {
            return lhs.offset_ < rhs.offset_;
          }
          if (lhs.count_ != rhs.count_) {
            return lhs.count_ < rhs.count_;
          }
          return lhs.type_ < rhs.type_;
        }
      };

     private:
      GLuint offset_;
      GLsizei count_;
      GLenum type_;
    };

    ~Buffer();

    GLenum target() const {
      return target_;
    }

    void set_target(GLenum target) {
      DCHECK_EQ(target_, 0u);  // you can only set this once.
      target_ = target;
    }

    bool shadowed() const {
      return shadowed_;
    }

    void MarkAsDeleted() {
      deleted_ = true;
    }

    void SetInfo(GLsizeiptr size, GLenum usage, bool shadow);

    // Clears any cache of index ranges.
    void ClearCache();

    // Check if an offset, size range is valid for the current buffer.
    bool CheckRange(GLintptr offset, GLsizeiptr size) const;

    // The manager that owns this Buffer.
    BufferManager* manager_;

    // True if deleted.
    bool deleted_;

    // Service side buffer id.
    GLuint service_id_;

    // The type of buffer. 0 = unset, GL_BUFFER_ARRAY = vertex data,
    // GL_ELEMENT_BUFFER_ARRAY = index data.
    // Once set a buffer can not be used for something else.
    GLenum target_;

    // Size of buffer.
    GLsizeiptr size_;

    // Usage of buffer.
    GLenum usage_;

    // Whether or not the data is shadowed.
    bool shadowed_;

    // A copy of the data in the buffer. This data is only kept if the target
    // is backed_ = true.
    scoped_array<int8> shadow_;

    // A map of ranges to the highest value in that range of a certain type.
    typedef std::map<Range, GLuint, Range::Less> RangeToMaxValueMap;
    RangeToMaxValueMap range_set_;
  };

  BufferManager(MemoryTracker* memory_tracker);
  ~BufferManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a Buffer for the given buffer.
  void CreateBuffer(GLuint client_id, GLuint service_id);

  // Gets the buffer info for the given buffer.
  Buffer* GetBuffer(GLuint client_id);

  // Removes a buffer info for the given buffer.
  void RemoveBuffer(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

  // Sets the size and usage of a buffer.
  void SetInfo(Buffer* info, GLsizeiptr size, GLenum usage);

  // Sets the target of a buffer. Returns false if the target can not be set.
  bool SetTarget(Buffer* info, GLenum target);

  void set_allow_buffers_on_multiple_targets(bool allow) {
    allow_buffers_on_multiple_targets_ = allow;
  }

  size_t mem_represented() const {
    return memory_tracker_->GetMemRepresented();
  }

 private:
  void StartTracking(Buffer* info);
  void StopTracking(Buffer* info);

  scoped_ptr<MemoryTypeTracker> memory_tracker_;

  // Info for each buffer in the system.
  typedef base::hash_map<GLuint, scoped_refptr<Buffer> > BufferInfoMap;
  BufferInfoMap buffer_infos_;

  // Whether or not buffers can be bound to multiple targets.
  bool allow_buffers_on_multiple_targets_;

  // Counts the number of Buffer allocated with 'this' as its manager.
  // Allows to check no Buffer will outlive this.
  unsigned int buffer_info_count_;

  bool have_context_;

  DISALLOW_COPY_AND_ASSIGN(BufferManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_BUFFER_MANAGER_H_
