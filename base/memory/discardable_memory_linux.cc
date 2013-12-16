// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_emulated.h"

namespace base {

// static
bool DiscardableMemory::SupportedNatively() {
  return false;
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemory(
    size_t size) {
  scoped_ptr<internal::DiscardableMemoryEmulated> memory(
      new internal::DiscardableMemoryEmulated(size));
  if (!memory->Initialize())
    return scoped_ptr<DiscardableMemory>();

  return memory.PassAs<DiscardableMemory>();
}

// static
bool DiscardableMemory::PurgeForTestingSupported() {
  return true;
}

// static
void DiscardableMemory::PurgeForTesting() {
  internal::DiscardableMemoryEmulated::PurgeForTesting();
}

}  // namespace base
