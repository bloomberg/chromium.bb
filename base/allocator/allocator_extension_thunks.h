// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_ALLOCATOR_EXTENSION_THUNKS_H_
#define BASE_ALLOCATOR_ALLOCATOR_EXTENSION_THUNKS_H_

#include <stddef.h> // for size_t

namespace base {
namespace allocator {
namespace thunks {

// WARNING: You probably don't want to use this file unless you are routing a
// new allocator extension from a specific allocator implementation to base.
// See allocator_extension.h to see the interface that base exports.

typedef void (*ReleaseFreeMemoryFunction)();
void SetReleaseFreeMemoryFunction(
    ReleaseFreeMemoryFunction release_free_memory_function);
ReleaseFreeMemoryFunction GetReleaseFreeMemoryFunction();

typedef bool (*GetNumericPropertyFunction)(const char* name, size_t* value);
void SetGetNumericPropertyFunction(
    GetNumericPropertyFunction get_numeric_property_function);
GetNumericPropertyFunction GetGetNumericPropertyFunction();

}  // namespace thunks
}  // namespace allocator
}  // namespace base

#endif  // BASE_ALLOCATOR_ALLOCATOR_EXTENSION_THUNKS_H_
