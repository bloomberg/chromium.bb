// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SCHEDULING_PRIORITY_H_
#define GPU_COMMAND_BUFFER_COMMON_SCHEDULING_PRIORITY_H_

#include "gpu/gpu_export.h"

namespace gpu {

enum class SchedulingPriority {
  // The Highest and High priorities can be used by priveleged clients only.
  // This priority should be used for UI contexts.
  kHighest,
  // This priority is used by the scheduler for prioritizing contexts which have
  // outstanding sync token waits.
  kHigh,
  // The following priorities can be used on unprivileged clients.
  // This priority should be used as the default priority for all contexts.
  kNormal,
  kLow,
  // This priority should be used for worker contexts.
  kLowest,
  kLast = kLowest
};

GPU_EXPORT const char* SchedulingPriorityToString(SchedulingPriority priority);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SCHEDULING_PRIORITY_H_
