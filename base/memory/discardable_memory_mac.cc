// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include <mach/mach.h>
#include <sys/mman.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

namespace base {
namespace {

// The VM subsystem allows tagging of memory and 240-255 is reserved for
// application use (see mach/vm_statistics.h). Pick 252 (after chromium's atomic
// weight of ~52).
const int kDiscardableMemoryTag = VM_MAKE_TAG(252);

class DiscardableMemoryMac : public DiscardableMemory {
 public:
  DiscardableMemoryMac(void* memory, size_t size)
      : memory_(memory),
        size_(size) {
    DCHECK(memory_);
  }

  virtual ~DiscardableMemoryMac() {
    vm_deallocate(mach_task_self(),
                  reinterpret_cast<vm_address_t>(memory_),
                  size_);
  }

  virtual LockDiscardableMemoryStatus Lock() OVERRIDE {
    DCHECK_EQ(0, mprotect(memory_, size_, PROT_READ | PROT_WRITE));
    int state = VM_PURGABLE_NONVOLATILE;
    kern_return_t ret = vm_purgable_control(
        mach_task_self(),
        reinterpret_cast<vm_address_t>(memory_),
        VM_PURGABLE_SET_STATE,
        &state);
    if (ret != KERN_SUCCESS)
      return DISCARDABLE_MEMORY_FAILED;

    return state & VM_PURGABLE_EMPTY ? DISCARDABLE_MEMORY_PURGED
                                     : DISCARDABLE_MEMORY_SUCCESS;
  }

  virtual void Unlock() OVERRIDE {
    int state = VM_PURGABLE_VOLATILE | VM_VOLATILE_GROUP_DEFAULT;
    kern_return_t ret = vm_purgable_control(
        mach_task_self(),
        reinterpret_cast<vm_address_t>(memory_),
        VM_PURGABLE_SET_STATE,
        &state);
    DCHECK_EQ(0, mprotect(memory_, size_, PROT_NONE));
    if (ret != KERN_SUCCESS)
      DLOG(ERROR) << "Failed to unlock memory.";
  }

  virtual void* Memory() const OVERRIDE {
    return memory_;
  }

 private:
  void* const memory_;
  const size_t size_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryMac);
};

}  // namespace

// static
bool DiscardableMemory::SupportedNatively() {
  return true;
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemory(
    size_t size) {
  vm_address_t buffer = 0;
  kern_return_t ret = vm_allocate(mach_task_self(),
                                  &buffer,
                                  size,
                                  VM_FLAGS_PURGABLE |
                                  VM_FLAGS_ANYWHERE |
                                  kDiscardableMemoryTag);
  if (ret != KERN_SUCCESS) {
    DLOG(ERROR) << "vm_allocate() failed";
    return scoped_ptr<DiscardableMemory>();
  }
  return scoped_ptr<DiscardableMemory>(
      new DiscardableMemoryMac(reinterpret_cast<void*>(buffer), size));
}

// static
bool DiscardableMemory::PurgeForTestingSupported() {
  return true;
}

// static
void DiscardableMemory::PurgeForTesting() {
  int state = 0;
  vm_purgable_control(mach_task_self(), 0, VM_PURGABLE_PURGE_ALL, &state);
}

}  // namespace base
