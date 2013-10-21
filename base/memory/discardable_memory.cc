// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace base {

// Stub implementations for platforms that don't support discardable memory.

#if !defined(OS_ANDROID) && !defined(OS_MACOSX)

// static
bool DiscardableMemory::Supported() {
  return false;
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemory(
    size_t size) {
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

#endif  // OS_*

}  // namespace base
