// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_extension.h"

#include "base/logging.h"

namespace base {
namespace allocator {

void ReleaseFreeMemory() {
  thunks::ReleaseFreeMemoryFunction release_free_memory_function =
      thunks::GetReleaseFreeMemoryFunction();
  if (release_free_memory_function)
    release_free_memory_function();
}

void SetReleaseFreeMemoryFunction(
    thunks::ReleaseFreeMemoryFunction release_free_memory_function) {
  DCHECK_EQ(thunks::GetReleaseFreeMemoryFunction(),
            reinterpret_cast<thunks::ReleaseFreeMemoryFunction>(NULL));
  thunks::SetReleaseFreeMemoryFunction(release_free_memory_function);
}

void SetGetNumericPropertyFunction(
    thunks::GetNumericPropertyFunction get_numeric_property_function) {
  DCHECK_EQ(thunks::GetGetNumericPropertyFunction(),
            reinterpret_cast<thunks::GetNumericPropertyFunction>(NULL));
  thunks::SetGetNumericPropertyFunction(get_numeric_property_function);
}

}  // namespace allocator
}  // namespace base
