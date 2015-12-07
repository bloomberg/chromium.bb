// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_ALLOCATOR_EXTENSION_H_
#define BASE_ALLOCATOR_ALLOCATOR_EXTENSION_H_

#include <stddef.h> // for size_t

#include "base/base_export.h"
#include "build/build_config.h"

namespace base {
namespace allocator {

typedef void (*ReleaseFreeMemoryFunction)();
typedef bool (*GetNumericPropertyFunction)(const char* name, size_t* value);

// Request that the allocator release any free memory it knows about to the
// system.
BASE_EXPORT void ReleaseFreeMemory();

// Get the named property's |value|. Returns true if the property is known.
// Returns false if the property is not a valid property name for the current
// allocator implementation.
// |name| or |value| cannot be NULL
BASE_EXPORT bool GetNumericProperty(const char* name, size_t* value);

// These settings allow specifying a callback used to implement the allocator
// extension functions.  These are optional, but if set they must only be set
// once.  These will typically called in an allocator-specific initialization
// routine.
//
// No threading promises are made.  The caller is responsible for making sure
// these pointers are set before any other threads attempt to call the above
// functions.

BASE_EXPORT void SetReleaseFreeMemoryFunction(
    ReleaseFreeMemoryFunction release_free_memory_function);

BASE_EXPORT void SetGetNumericPropertyFunction(
    GetNumericPropertyFunction get_numeric_property_function);

}  // namespace allocator
}  // namespace base

#endif  // BASE_ALLOCATOR_ALLOCATOR_EXTENSION_H_
