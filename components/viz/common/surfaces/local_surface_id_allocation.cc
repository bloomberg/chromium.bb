// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/local_surface_id_allocation.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"

namespace viz {

LocalSurfaceIdAllocation::LocalSurfaceIdAllocation(
    const LocalSurfaceId& local_surface_id,
    base::TimeTicks allocation_time)
    : local_surface_id_(local_surface_id), allocation_time_(allocation_time) {}

bool LocalSurfaceIdAllocation::IsValid() const {
  return local_surface_id_.is_valid() && !allocation_time_.is_null();
}

std::string LocalSurfaceIdAllocation::ToString() const {
  return base::StringPrintf(
      "LocalSurfaceIdAllocation(%s, AllocationTime(%" PRId64 "))",
      local_surface_id_.ToString().c_str(),
      (allocation_time_ - base::TimeTicks()).InMilliseconds());
}

std::ostream& operator<<(std::ostream& stream,
                         const LocalSurfaceIdAllocation& allocation) {
  return stream << allocation.ToString();
}

}  // namespace viz
