// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_SHARED_MEMORY_UTIL_H_
#define BASE_TEST_TEST_SHARED_MEMORY_UTIL_H_

#include "base/memory/shared_memory_handle.h"

namespace base {

// Check that the shared memory |handle| cannot be used to perform
// a writable mapping with low-level system APIs like mmap(). Return true
// in case of success (i.e. writable mappings are _not_ allowed), or false
// otherwise.
bool CheckReadOnlySharedMemoryHandleForTesting(SharedMemoryHandle handle);

}  // namespace base

#endif  // BASE_TEST_TEST_SHARED_MEMORY_UTIL_H_
