// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_UMA_STATS_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_UMA_STATS_H_

#include <stddef.h>

namespace content {

// Memory usage statistics send periodically to the browser process to report
// in UMA histograms if the GPU process crashes.
struct GPUMemoryUmaStats {
  GPUMemoryUmaStats()
      : bytes_allocated_current(0),
        bytes_allocated_max(0),
        context_group_count(0) {
  }

  // The number of bytes currently allocated.
  uint32_t bytes_allocated_current;

  // The maximum number of bytes ever allocated at once.
  uint32_t bytes_allocated_max;

  // The number of context groups.
  uint32_t context_group_count;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_MEMORY_UMA_STATS_H_
