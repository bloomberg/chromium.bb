// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_ALLOCATOR_INTERCEPTION_MAC_H_
#define BASE_ALLOCATOR_ALLOCATOR_INTERCEPTION_MAC_H_

#include <stddef.h>

namespace base {
namespace allocator {

// Calls the original implementation of malloc/calloc prior to interception.
bool UncheckedMallocMac(size_t size, void** result);
bool UncheckedCallocMac(size_t num_items, size_t size, void** result);

// Intercepts calls to default and purgeable malloc zones. Intercepts Core
// Foundation and Objective-C allocations.
void InterceptAllocationsMac();
}  // namespace allocator
}  // namespace base

#endif  // BASE_ALLOCATOR_ALLOCATOR_INTERCEPTION_MAC_H_
