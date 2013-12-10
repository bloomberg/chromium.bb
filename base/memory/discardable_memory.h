// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_H_

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace base {

enum LockDiscardableMemoryStatus {
  DISCARDABLE_MEMORY_FAILED = -1,
  DISCARDABLE_MEMORY_PURGED = 0,
  DISCARDABLE_MEMORY_SUCCESS = 1
};

// Platform abstraction for discardable memory. DiscardableMemory is used to
// cache large objects without worrying about blowing out memory, both on mobile
// devices where there is no swap, and desktop devices where unused free memory
// should be used to help the user experience. This is preferable to releasing
// memory in response to an OOM signal because it is simpler, though it has less
// flexibility as to which objects get discarded.
//
// Discardable memory has two states: locked and unlocked. While the memory is
// locked, it will not be discarded. Unlocking the memory allows the OS to
// reclaim it if needed. Locks do not nest.
//
// Notes:
//   - The paging behavior of memory while it is locked is not specified. While
//     mobile platforms will not swap it out, it may qualify for swapping
//     on desktop platforms. It is not expected that this will matter, as the
//     preferred pattern of usage for DiscardableMemory is to lock down the
//     memory, use it as quickly as possible, and then unlock it.
//   - Because of memory alignment, the amount of memory allocated can be
//     larger than the requested memory size. It is not very efficient for
//     small allocations.
//   - A discardable memory instance is not thread safe. It is the
//     responsibility of users of discardable memory to ensure there are no
//     races.
//
// References:
//   - Linux: http://lwn.net/Articles/452035/
//   - Mac: http://trac.webkit.org/browser/trunk/Source/WebCore/platform/mac/PurgeableBufferMac.cpp
//          the comment starting with "vm_object_purgable_control" at
//            http://www.opensource.apple.com/source/xnu/xnu-792.13.8/osfmk/vm/vm_object.c
//
// Thread-safety: DiscardableMemory instances are not thread-safe.
class BASE_EXPORT DiscardableMemory {
 public:
  virtual ~DiscardableMemory() {}

  // Check whether the system supports discardable memory natively. Returns
  // false if the support is emulated.
  static bool SupportedNatively();

  static scoped_ptr<DiscardableMemory> CreateLockedMemory(size_t size);

  // Locks the memory so that it will not be purged by the system. Returns
  // DISCARDABLE_MEMORY_SUCCESS on success. If the return value is
  // DISCARDABLE_MEMORY_FAILED then this object should be discarded and
  // a new one should be created. If the return value is
  // DISCARDABLE_MEMORY_PURGED then the memory is present but any data that
  // was in it is gone.
  virtual LockDiscardableMemoryStatus Lock() WARN_UNUSED_RESULT = 0;

  // Unlocks the memory so that it can be purged by the system. Must be called
  // after every successful lock call.
  virtual void Unlock() = 0;

  // Returns the memory address held by this object. The object must be locked
  // before calling this. Otherwise, this will cause a DCHECK error.
  virtual void* Memory() const = 0;

  // Testing utility calls.

  // Check whether a purge of all discardable memory in the system is supported.
  // Use only for testing!
  static bool PurgeForTestingSupported();

  // Purge all discardable memory in the system. This call has global effects
  // across all running processes, so it should only be used for testing!
  static void PurgeForTesting();
};

}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_H_
