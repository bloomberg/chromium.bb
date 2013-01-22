// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_H_

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"

namespace base {

enum LockDiscardableMemoryStatus {
  FAILED = -1,
  PURGED = 0,
  SUCCESS = 1
};

// Platform abstraction for discardable memory. The DiscardableMemory should
// be mainly used for mobile devices where VM swap is not available, such as
// android. It is particularly helpful for caching large objects without
// worrying about OOM situation.
// Discardable memory allows user to lock(pin) and unlock(unpin) pages so that
// the allocated memory can be discarded by the kernel under memory pressure.
// From http://lwn.net/Articles/452035/: "Pinned pages (the default) behave
// like any anonymous memory. Unpinned pages are available to the kernel for
// eviction during VM pressure. When repinning the pages, the return value
// instructs user-space as to any eviction. In this manner, user-space processes
// may implement caching and similar resource management that efficiently
// integrates with kernel memory management."
// Compared to relying on OOM signals to clean up the memory, DiscardableMemory
// is much simpler as the OS will taking care of the LRU algorithm and there
// is no need to implement a separate cleanup() call. However, there is no
// guarantee which cached objects will be discarded.
// Because of memory alignment, the actual locked discardable memory could be
// larger than the requested memory size. It may not be very efficient for
// small size allocations.
class BASE_EXPORT DiscardableMemory {
 public:
  DiscardableMemory();

  // If the discardable memory is locked, the destructor will unlock it.
  // The opened file will also be closed after this.
  ~DiscardableMemory();

  // Check whether the system supports discardable memory.
  static bool Supported() {
#if defined(OS_ANDROID)
    return true;
#endif
    return false;
  }

  // Initialize the DiscardableMemory object. On success, this function returns
  // true and the memory is locked. This should only be called once.
  bool InitializeAndLock(size_t size);

  // Lock the memory so that it will not be purged by the system. Returns
  // SUCCESS on success. Returns FAILED on error or PURGED if the memory is
  // purged. Don't call this function in a nested fashion.
  LockDiscardableMemoryStatus Lock() WARN_UNUSED_RESULT;

  // Unlock the memory so that it can be purged by the system. Must be called
  // after every successful lock call.
  void Unlock();

  // Return the memory address held by this object. When this object is locked,
  // this call will return the address in caller's address space. Otherwise,
  // this call returns NULL.
  void* Memory() const;

 private:
  // Maps the discardable memory into the caller's address space.
  // Returns true on success, false otherwise.
  bool Map();

  // Unmaps the discardable memory from the caller's address space.
  void Unmap();

  int fd_;
  void* memory_;
  size_t size_;
  bool is_pinned_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemory);
};

}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_H_
