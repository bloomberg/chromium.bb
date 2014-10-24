// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_emulated.h"

#include "base/lazy_instance.h"
#include "base/memory/discardable_memory_manager.h"

namespace base {
namespace {

// This is admittedly pretty magical.
const size_t kEmulatedMemoryLimit = 512 * 1024 * 1024;
const size_t kEmulatedSoftMemoryLimit = 32 * 1024 * 1024;
const size_t kEmulatedHardMemoryLimitExpirationTimeMs = 1000;

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
        kEmulatedMemoryLimit,
        kEmulatedSoftMemoryLimit,
        TimeDelta::FromMilliseconds(kEmulatedHardMemoryLimitExpirationTimeMs));
  }
  static void Delete(internal::DiscardableMemoryManager* instance) {
    instance->~DiscardableMemoryManager();
  }
};

LazyInstance<internal::DiscardableMemoryManager,
             DiscardableMemoryManagerLazyInstanceTraits>
    g_manager = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace internal {

DiscardableMemoryEmulated::DiscardableMemoryEmulated(size_t bytes)
    : bytes_(bytes),
      is_locked_(false) {
  g_manager.Pointer()->Register(this, bytes);
}

DiscardableMemoryEmulated::~DiscardableMemoryEmulated() {
  if (is_locked_)
    Unlock();
  g_manager.Pointer()->Unregister(this);
}

// static
bool DiscardableMemoryEmulated::ReduceMemoryUsage() {
  return g_manager.Pointer()->ReduceMemoryUsage();
}

// static
void DiscardableMemoryEmulated::ReduceMemoryUsageUntilWithinLimit(
    size_t bytes) {
  g_manager.Pointer()->ReduceMemoryUsageUntilWithinLimit(bytes);
}

// static
void DiscardableMemoryEmulated::PurgeForTesting() {
  g_manager.Pointer()->PurgeAll();
}

bool DiscardableMemoryEmulated::Initialize() {
  return Lock() != DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;
}

DiscardableMemoryLockStatus DiscardableMemoryEmulated::Lock() {
  DCHECK(!is_locked_);

  bool purged = false;
  if (!g_manager.Pointer()->AcquireLock(this, &purged))
    return DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;

  is_locked_ = true;
  return purged ? DISCARDABLE_MEMORY_LOCK_STATUS_PURGED
                : DISCARDABLE_MEMORY_LOCK_STATUS_SUCCESS;
}

void DiscardableMemoryEmulated::Unlock() {
  DCHECK(is_locked_);
  g_manager.Pointer()->ReleaseLock(this);
  is_locked_ = false;
}

void* DiscardableMemoryEmulated::Memory() const {
  DCHECK(is_locked_);
  DCHECK(memory_);
  return memory_.get();
}

bool DiscardableMemoryEmulated::AllocateAndAcquireLock() {
  if (memory_)
    return true;

  memory_.reset(new uint8[bytes_]);
  return false;
}

void DiscardableMemoryEmulated::Purge() {
  memory_.reset();
}

bool DiscardableMemoryEmulated::IsMemoryResident() const {
  return true;
}

}  // namespace internal
}  // namespace base
