// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include <mach/mach.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_vm.h"
#include "base/memory/discardable_memory_emulated.h"
#include "base/memory/discardable_memory_malloc.h"
#include "base/memory/scoped_ptr.h"

namespace base {
namespace {

// The VM subsystem allows tagging of memory and 240-255 is reserved for
// application use (see mach/vm_statistics.h). Pick 252 (after chromium's atomic
// weight of ~52).
const int kDiscardableMemoryTag = VM_MAKE_TAG(252);

class DiscardableMemoryMac : public DiscardableMemory {
 public:
  explicit DiscardableMemoryMac(size_t size)
      : memory_(0, 0),
        size_(mach_vm_round_page(size)) {
  }

  bool Initialize() {
    DCHECK_EQ(memory_.size(), 0u);
    vm_address_t address = 0;
    kern_return_t ret = vm_allocate(mach_task_self(),
                                    &address,
                                    size_,
                                    VM_FLAGS_PURGABLE |
                                        VM_FLAGS_ANYWHERE |
                                        kDiscardableMemoryTag);
    if (ret != KERN_SUCCESS) {
      MACH_DLOG(ERROR, ret) << "vm_allocate";
      return false;
    }

    memory_.reset(address, size_);

    return true;
  }

  virtual ~DiscardableMemoryMac() {
  }

  virtual DiscardableMemoryLockStatus Lock() OVERRIDE {
    kern_return_t ret;
    MACH_DCHECK((ret = vm_protect(mach_task_self(),
                                  memory_.address(),
                                  memory_.size(),
                                  FALSE,
                                  VM_PROT_DEFAULT)) == KERN_SUCCESS, ret);
    int state = VM_PURGABLE_NONVOLATILE;
    ret = vm_purgable_control(mach_task_self(),
                              memory_.address(),
                              VM_PURGABLE_SET_STATE,
                              &state);
    if (ret != KERN_SUCCESS)
      return DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;

    return state & VM_PURGABLE_EMPTY ? DISCARDABLE_MEMORY_LOCK_STATUS_PURGED
                                     : DISCARDABLE_MEMORY_LOCK_STATUS_SUCCESS;
  }

  virtual void Unlock() OVERRIDE {
    int state = VM_PURGABLE_VOLATILE | VM_VOLATILE_GROUP_DEFAULT;
    kern_return_t ret = vm_purgable_control(mach_task_self(),
                                            memory_.address(),
                                            VM_PURGABLE_SET_STATE,
                                            &state);
    MACH_DLOG_IF(ERROR, ret != KERN_SUCCESS, ret) << "vm_purgable_control";
    MACH_DCHECK((ret = vm_protect(mach_task_self(),
                                  memory_.address(),
                                  memory_.size(),
                                  FALSE,
                                  VM_PROT_NONE)) == KERN_SUCCESS, ret);
  }

  virtual void* Memory() const OVERRIDE {
    return reinterpret_cast<void*>(memory_.address());
  }

 private:
  mac::ScopedMachVM memory_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryMac);
};

}  // namespace

// static
void DiscardableMemory::RegisterMemoryPressureListeners() {
  internal::DiscardableMemoryEmulated::RegisterMemoryPressureListeners();
}

// static
void DiscardableMemory::UnregisterMemoryPressureListeners() {
  internal::DiscardableMemoryEmulated::UnregisterMemoryPressureListeners();
}

// static
void DiscardableMemory::GetSupportedTypes(
    std::vector<DiscardableMemoryType>* types) {
  const DiscardableMemoryType supported_types[] = {
    DISCARDABLE_MEMORY_TYPE_MAC,
    DISCARDABLE_MEMORY_TYPE_EMULATED,
    DISCARDABLE_MEMORY_TYPE_MALLOC
  };
  types->assign(supported_types, supported_types + arraysize(supported_types));
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemoryWithType(
    DiscardableMemoryType type, size_t size) {
  switch (type) {
    case DISCARDABLE_MEMORY_TYPE_NONE:
    case DISCARDABLE_MEMORY_TYPE_ASHMEM:
      return scoped_ptr<DiscardableMemory>();
    case DISCARDABLE_MEMORY_TYPE_MAC: {
      scoped_ptr<DiscardableMemoryMac> memory(new DiscardableMemoryMac(size));
      if (!memory->Initialize())
        return scoped_ptr<DiscardableMemory>();

      return memory.PassAs<DiscardableMemory>();
    }
    case DISCARDABLE_MEMORY_TYPE_EMULATED: {
      scoped_ptr<internal::DiscardableMemoryEmulated> memory(
          new internal::DiscardableMemoryEmulated(size));
      if (!memory->Initialize())
        return scoped_ptr<DiscardableMemory>();

      return memory.PassAs<DiscardableMemory>();
    }
    case DISCARDABLE_MEMORY_TYPE_MALLOC: {
      scoped_ptr<internal::DiscardableMemoryMalloc> memory(
          new internal::DiscardableMemoryMalloc(size));
      if (!memory->Initialize())
        return scoped_ptr<DiscardableMemory>();

      return memory.PassAs<DiscardableMemory>();
    }
  }

  NOTREACHED();
  return scoped_ptr<DiscardableMemory>();
}

// static
void DiscardableMemory::PurgeForTesting() {
  int state = 0;
  vm_purgable_control(mach_task_self(), 0, VM_PURGABLE_PURGE_ALL, &state);
  internal::DiscardableMemoryEmulated::PurgeForTesting();
}

}  // namespace base
