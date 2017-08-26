// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_FLUSH_PARAMS_H_
#define GPU_IPC_COMMON_FLUSH_PARAMS_H_

#include <stdint.h>
#include <vector>

#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/gpu_export.h"
#include "ui/latency/latency_info.h"

namespace gpu {

struct FlushParams {
  FlushParams();
  FlushParams(const FlushParams& other);
  FlushParams(FlushParams&& other);
  ~FlushParams();
  FlushParams& operator=(const FlushParams& other);
  FlushParams& operator=(FlushParams&& other);

  // Route ID of the command buffer for this flush.
  int32_t route_id;
  // Client put offset. Service get offset is updated in shared memory.
  int32_t put_offset;
  // Increasing counter for the flush.
  uint32_t flush_id;
  // Latency timestamps used for correlating swap buffers in the GPU process
  // with input events.
  std::vector<ui::LatencyInfo> latency_info;
  // Sync token dependencies of the flush. These are sync tokens for which waits
  // are in the commands that are part of this flush.
  std::vector<SyncToken> sync_token_fences;
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_FLUSH_PARAMS_H_
