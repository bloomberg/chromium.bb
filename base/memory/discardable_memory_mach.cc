// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_mach.h"

#include <mach/mach.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/mach_logging.h"

namespace base {
namespace {

// For Mach, have the DiscardableMemoryManager trigger userspace eviction when
// address space usage gets too high (e.g. 512 MBytes).
const size_t kMachMemoryLimit = 512 * 1024 * 1024;

// internal::DiscardableMemoryManager has an explicit constructor that takes
// a number of memory limit parameters. The LeakyLazyInstanceTraits doesn't
// handle the case. Thus, we need our own class here.
struct DiscardableMemoryManagerLazyInstanceTraits {
  // Leaky as discardable memory clients can use this after the exit handler
  // has been called.
  static const bool kRegisterOnExit = false;
#ifndef NDEBUG
  static const bool kAllowedToAccessOnNonjoinableThread = true;
#endif

  static internal::DiscardableMemoryManager* New(void* instance) {
    return new (instance) internal::DiscardableMemoryManager(
        kMachMemoryLimit, kMachMemoryLimit, TimeDelta::Max());
  }
  static void Delete(internal::DiscardableMemoryManager* instance) {
    instance->~DiscardableMemoryManager();
  }
};

LazyInstance<internal::DiscardableMemoryManager,
             DiscardableMemoryManagerLazyInstanceTraits>
    g_manager = LAZY_INSTANCE_INITIALIZER;

// The VM subsystem allows tagging of memory and 240-255 is reserved for
// application use (see mach/vm_statistics.h). Pick 252 (after chromium's atomic
// weight of ~52).
const int kDiscardableMemoryTag = VM_MAKE_TAG(252);

}  // namespace

namespace internal {

DiscardableMemoryMach::DiscardableMemoryMach(size_t bytes)
    : memory_(0, 0), bytes_(mach_vm_round_page(bytes)), is_locked_(false) {
  g_manager.Pointer()->Register(this, bytes);
}

DiscardableMemoryMach::~DiscardableMemoryMach() {
  if (is_locked_)
    Unlock();
  g_manager.Pointer()->Unregister(this);
}

// static
void DiscardableMemoryMach::PurgeForTesting() {
  int state = 0;
  vm_purgable_control(mach_task_self(), 0, VM_PURGABLE_PURGE_ALL, &state);
}

bool DiscardableMemoryMach::Initialize() {
  return Lock() != DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;
}

DiscardableMemoryLockStatus DiscardableMemoryMach::Lock() {
  DCHECK(!is_locked_);

  bool purged = false;
  if (!g_manager.Pointer()->AcquireLock(this, &purged))
    return DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;

  is_locked_ = true;
  return purged ? DISCARDABLE_MEMORY_LOCK_STATUS_PURGED
                : DISCARDABLE_MEMORY_LOCK_STATUS_SUCCESS;
}

void DiscardableMemoryMach::Unlock() {
  DCHECK(is_locked_);
  g_manager.Pointer()->ReleaseLock(this);
  is_locked_ = false;
}

void* DiscardableMemoryMach::Memory() const {
  DCHECK(is_locked_);
  return reinterpret_cast<void*>(memory_.address());
}

bool DiscardableMemoryMach::AllocateAndAcquireLock() {
  kern_return_t ret;
  bool persistent;
  if (!memory_.size()) {
    vm_address_t address = 0;
    ret = vm_allocate(
        mach_task_self(),
        &address,
        bytes_,
        VM_FLAGS_ANYWHERE | VM_FLAGS_PURGABLE | kDiscardableMemoryTag);
    MACH_CHECK(ret == KERN_SUCCESS, ret) << "vm_allocate";
    memory_.reset(address, bytes_);

    // When making a fresh allocation, it's impossible for |persistent| to
    // be true.
    persistent = false;
  } else {
    // |persistent| will be reset to false below if appropriate, but when
    // reusing an existing allocation, it's possible for it to be true.
    persistent = true;

#if !defined(NDEBUG)
    ret = vm_protect(mach_task_self(),
                     memory_.address(),
                     memory_.size(),
                     FALSE,
                     VM_PROT_DEFAULT);
    MACH_DCHECK(ret == KERN_SUCCESS, ret) << "vm_protect";
#endif
  }

  int state = VM_PURGABLE_NONVOLATILE;
  ret = vm_purgable_control(
      mach_task_self(), memory_.address(), VM_PURGABLE_SET_STATE, &state);
  MACH_CHECK(ret == KERN_SUCCESS, ret) << "vm_purgable_control";
  if (state & VM_PURGABLE_EMPTY)
    persistent = false;

  return persistent;
}

void DiscardableMemoryMach::ReleaseLock() {
  int state = VM_PURGABLE_VOLATILE | VM_VOLATILE_GROUP_DEFAULT;
  kern_return_t ret = vm_purgable_control(
      mach_task_self(), memory_.address(), VM_PURGABLE_SET_STATE, &state);
  MACH_CHECK(ret == KERN_SUCCESS, ret) << "vm_purgable_control";

#if !defined(NDEBUG)
  ret = vm_protect(
      mach_task_self(), memory_.address(), memory_.size(), FALSE, VM_PROT_NONE);
  MACH_DCHECK(ret == KERN_SUCCESS, ret) << "vm_protect";
#endif
}

void DiscardableMemoryMach::Purge() {
  memory_.reset();
}

bool DiscardableMemoryMach::IsMemoryResident() const {
  return true;
}

}  // namespace internal
}  // namespace base
