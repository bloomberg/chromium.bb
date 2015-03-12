// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "base/logging.h"
#include "base/memory/discardable_memory_shmem.h"
#include "base/memory/scoped_ptr.h"

namespace base {

// static
void DiscardableMemory::GetSupportedTypes(
    std::vector<DiscardableMemoryType>* types) {
  const DiscardableMemoryType supported_types[] = {
    DISCARDABLE_MEMORY_TYPE_SHMEM
  };
  types->assign(supported_types, supported_types + arraysize(supported_types));
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemoryWithType(
    DiscardableMemoryType type, size_t size) {
  switch (type) {
    case DISCARDABLE_MEMORY_TYPE_SHMEM:
      return make_scoped_ptr(new internal::DiscardableMemoryShmem(size));
    case DISCARDABLE_MEMORY_TYPE_NONE:
      NOTREACHED();
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace base
