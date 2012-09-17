// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_

#include <string>
#include "base/basictypes.h"
#include "base/debug/trace_event.h"

namespace gpu {
namespace gles2 {

// A MemoryTracker is used to propagate per-ContextGroup memory usage
// statistics to the global GpuMemoryManager.
class MemoryTracker : public base::RefCounted<MemoryTracker> {
 public:
  virtual void TrackMemoryAllocatedChange(size_t old_size, size_t new_size) = 0;

 protected:
  friend class base::RefCounted<MemoryTracker>;
  MemoryTracker() {}
  virtual ~MemoryTracker() {};

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryTracker);
};

// A MemoryTypeTracker tracks the use of a particular type of memory (buffer,
// texture, or renderbuffer) and forward the result to a specified
// MemoryTracker.
class MemoryTypeTracker {
 public:
  MemoryTypeTracker(MemoryTracker* memory_tracker)
    : memory_tracker_(memory_tracker),
      has_updated_mem_represented_(false),
      last_updated_mem_represented_(0) {
  }

  void UpdateMemRepresented(size_t mem_represented) {
    if (!has_updated_mem_represented_ &&
        mem_represented == last_updated_mem_represented_) {
      return;
    }
    if (memory_tracker_) {
      memory_tracker_->TrackMemoryAllocatedChange(
        last_updated_mem_represented_,
        mem_represented);
    }
    has_updated_mem_represented_ = true;
    last_updated_mem_represented_ = mem_represented;
  }

  size_t GetMemRepresented() const {
    return last_updated_mem_represented_;
  }

 private:
  MemoryTracker* memory_tracker_;
  bool has_updated_mem_represented_;
  size_t last_updated_mem_represented_;

  DISALLOW_COPY_AND_ASSIGN(MemoryTypeTracker);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_
