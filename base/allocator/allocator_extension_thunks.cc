// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_extension_thunks.h"

#include <cstddef> // for NULL

namespace base {
namespace allocator {
namespace thunks {

// This slightly odd translation unit exists because of the peculularity of how
// allocator_unittests work on windows.  That target has to perform
// tcmalloc-specific initialization on windows, but it cannot depend on base
// otherwise. This target sits in the middle - base and allocator_unittests
// can depend on it. This file can't depend on anything else in base, including
// logging.

static ReleaseFreeMemoryFunction g_release_free_memory_function = NULL;
static GetNumericPropertyFunction g_get_numeric_property_function = NULL;

void SetReleaseFreeMemoryFunction(
    ReleaseFreeMemoryFunction release_free_memory_function) {
  g_release_free_memory_function = release_free_memory_function;
}

ReleaseFreeMemoryFunction GetReleaseFreeMemoryFunction() {
  return g_release_free_memory_function;
}

void SetGetNumericPropertyFunction(
    GetNumericPropertyFunction get_numeric_property_function) {
  g_get_numeric_property_function = get_numeric_property_function;
}

GetNumericPropertyFunction GetGetNumericPropertyFunction() {
  return g_get_numeric_property_function;
}

}  // namespace thunks
}  // namespace allocator
}  // namespace base
