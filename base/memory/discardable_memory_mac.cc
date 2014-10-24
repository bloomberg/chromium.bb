// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "base/logging.h"
#include "base/memory/discardable_memory_emulated.h"
#include "base/memory/discardable_memory_mach.h"
#include "base/memory/discardable_memory_manager.h"
#include "base/memory/discardable_memory_shmem.h"
#include "base/memory/scoped_ptr.h"

namespace base {

// static
void DiscardableMemory::ReleaseFreeMemory() {
  internal::DiscardableMemoryShmem::ReleaseFreeMemory();
}

// static
bool DiscardableMemory::ReduceMemoryUsage() {
  return internal::DiscardableMemoryEmulated::ReduceMemoryUsage();
}

// static
void DiscardableMemory::GetSupportedTypes(
    std::vector<DiscardableMemoryType>* types) {
  const DiscardableMemoryType supported_types[] = {
    DISCARDABLE_MEMORY_TYPE_MACH,
    DISCARDABLE_MEMORY_TYPE_EMULATED,
    DISCARDABLE_MEMORY_TYPE_SHMEM
  };
  types->assign(supported_types, supported_types + arraysize(supported_types));
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemoryWithType(
    DiscardableMemoryType type, size_t size) {
  switch (type) {
    case DISCARDABLE_MEMORY_TYPE_MACH: {
      scoped_ptr<internal::DiscardableMemoryMach> memory(
          new internal::DiscardableMemoryMach(size));
      if (!memory->Initialize())
        return nullptr;

      return memory.Pass();
    }
    case DISCARDABLE_MEMORY_TYPE_EMULATED: {
      scoped_ptr<internal::DiscardableMemoryEmulated> memory(
          new internal::DiscardableMemoryEmulated(size));
      if (!memory->Initialize())
        return nullptr;

      return memory.Pass();
    }
    case DISCARDABLE_MEMORY_TYPE_SHMEM: {
      scoped_ptr<internal::DiscardableMemoryShmem> memory(
          new internal::DiscardableMemoryShmem(size));
      if (!memory->Initialize())
        return nullptr;

      return memory.Pass();
    }
    case DISCARDABLE_MEMORY_TYPE_NONE:
    case DISCARDABLE_MEMORY_TYPE_ASHMEM:
      NOTREACHED();
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

// static
void DiscardableMemory::PurgeForTesting() {
  internal::DiscardableMemoryMach::PurgeForTesting();
  internal::DiscardableMemoryEmulated::PurgeForTesting();
  internal::DiscardableMemoryShmem::PurgeForTesting();
}

}  // namespace base
