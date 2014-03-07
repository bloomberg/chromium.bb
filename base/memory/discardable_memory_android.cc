// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "base/android/sys_utils.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/discardable_memory_allocator_android.h"
#include "base/memory/discardable_memory_emulated.h"
#include "base/memory/discardable_memory_malloc.h"

namespace base {
namespace {

const char kAshmemAllocatorName[] = "DiscardableMemoryAllocator";

struct DiscardableMemoryAllocatorWrapper {
  DiscardableMemoryAllocatorWrapper()
      : allocator(kAshmemAllocatorName,
                  GetOptimalAshmemRegionSizeForAllocator()) {
  }

  internal::DiscardableMemoryAllocator allocator;

 private:
  // Returns 64 MBytes for a 512 MBytes device, 128 MBytes for 1024 MBytes...
  static size_t GetOptimalAshmemRegionSizeForAllocator() {
    // Note that this may do some I/O (without hitting the disk though) so it
    // should not be called on the critical path.
    return base::android::SysUtils::AmountOfPhysicalMemoryKB() * 1024 / 8;
  }
};

LazyInstance<DiscardableMemoryAllocatorWrapper>::Leaky g_context =
    LAZY_INSTANCE_INITIALIZER;

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
    DISCARDABLE_MEMORY_TYPE_ANDROID,
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
    case DISCARDABLE_MEMORY_TYPE_MAC:
      return scoped_ptr<DiscardableMemory>();
    case DISCARDABLE_MEMORY_TYPE_ANDROID: {
      return g_context.Pointer()->allocator.Allocate(size);
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
bool DiscardableMemory::PurgeForTestingSupported() {
  return false;
}

// static
void DiscardableMemory::PurgeForTesting() {
  NOTIMPLEMENTED();
}

}  // namespace base
