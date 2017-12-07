// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"

namespace gpu {
namespace gles2 {

// A MemoryTracker is used to propagate per-ContextGroup memory usage
// statistics to the global GpuMemoryManager.
class MemoryTracker : public base::RefCounted<MemoryTracker> {
 public:
   virtual void TrackMemoryAllocatedChange(size_t old_size,
                                           size_t new_size) = 0;

   // Ensure a certain amount of GPU memory is free. Returns true on success.
   virtual bool EnsureGPUMemoryAvailable(size_t size_needed) = 0;

   // Tracing id which identifies the GPU client for whom memory is being
   // allocated.
   virtual uint64_t ClientTracingId() const = 0;

   // Identifies the share group within which memory is being allocated.
   virtual uint64_t ShareGroupTracingGUID() const = 0;

   // Raw ID identifying the GPU client for whom memory is being allocated.
   virtual int ClientId() const = 0;

 protected:
  friend class base::RefCounted<MemoryTracker>;
  MemoryTracker() = default;
  virtual ~MemoryTracker() = default;

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
      has_done_update_(false),
      mem_represented_(0),
      mem_represented_at_last_update_(0) {
    UpdateMemRepresented();
  }

  ~MemoryTypeTracker() {
    UpdateMemRepresented();
  }

  void TrackMemAlloc(size_t bytes) {
    mem_represented_ += bytes;
    UpdateMemRepresented();
  }

  void TrackMemFree(size_t bytes) {
    DCHECK(bytes <= mem_represented_);
    mem_represented_ -= bytes;
    UpdateMemRepresented();
  }

  size_t GetMemRepresented() const {
    return mem_represented_at_last_update_;
  }

  // Ensure a certain amount of GPU memory is free. Returns true on success.
  bool EnsureGPUMemoryAvailable(size_t size_needed) {
    if (memory_tracker_) {
      return memory_tracker_->EnsureGPUMemoryAvailable(size_needed);
    }
    return true;
  }

 private:
  void UpdateMemRepresented() {
    // Skip redundant updates only if we have already done an update.
    if (!has_done_update_ &&
        mem_represented_ == mem_represented_at_last_update_) {
      return;
    }
    if (memory_tracker_) {
      memory_tracker_->TrackMemoryAllocatedChange(
        mem_represented_at_last_update_,
        mem_represented_);
    }
    has_done_update_ = true;
    mem_represented_at_last_update_ = mem_represented_;
  }

  MemoryTracker* memory_tracker_;
  bool has_done_update_;
  size_t mem_represented_;
  size_t mem_represented_at_last_update_;

  DISALLOW_COPY_AND_ASSIGN(MemoryTypeTracker);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MEMORY_TRACKING_H_
