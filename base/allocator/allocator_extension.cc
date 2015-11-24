// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_extension.h"

#include "base/logging.h"

namespace base {
namespace allocator {

void GetStats(char* buffer, int buffer_length) {
  DCHECK_GT(buffer_length, 0);
  thunks::GetStatsFunction get_stats_function = thunks::GetGetStatsFunction();
  if (get_stats_function)
    get_stats_function(buffer, buffer_length);
  else
    buffer[0] = '\0';
}

void ReleaseFreeMemory() {
  thunks::ReleaseFreeMemoryFunction release_free_memory_function =
      thunks::GetReleaseFreeMemoryFunction();
  if (release_free_memory_function)
    release_free_memory_function();
}

void SetGetStatsFunction(thunks::GetStatsFunction get_stats_function) {
  DCHECK_EQ(thunks::GetGetStatsFunction(),
            reinterpret_cast<thunks::GetStatsFunction>(NULL));
  thunks::SetGetStatsFunction(get_stats_function);
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
