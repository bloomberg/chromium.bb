// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_extension.h"

#include "base/logging.h"

namespace base {
namespace allocator {

namespace {
ReleaseFreeMemoryFunction g_release_free_memory_function = nullptr;
GetNumericPropertyFunction g_get_numeric_property_function = nullptr;
}

void ReleaseFreeMemory() {
  if (g_release_free_memory_function)
    g_release_free_memory_function();
}

bool GetNumericProperty(const char* name, size_t* value) {
  return g_get_numeric_property_function &&
         g_get_numeric_property_function(name, value);
}

void SetReleaseFreeMemoryFunction(
    ReleaseFreeMemoryFunction release_free_memory_function) {
  DCHECK(!g_release_free_memory_function);
  g_release_free_memory_function = release_free_memory_function;
}

void SetGetNumericPropertyFunction(
    GetNumericPropertyFunction get_numeric_property_function) {
  DCHECK(!g_get_numeric_property_function);
  g_get_numeric_property_function = get_numeric_property_function;
}

}  // namespace allocator
}  // namespace base
