// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

#include "base/rand_util.h"

namespace viz {

ParentLocalSurfaceIdAllocator::ParentLocalSurfaceIdAllocator()
    : last_known_local_surface_id_(kInvalidParentSequenceNumber,
                                   kInitialChildSequenceNumber,
                                   base::UnguessableToken::Create()) {}

const LocalSurfaceId& ParentLocalSurfaceIdAllocator::UpdateFromChild(
    const LocalSurfaceId& child_allocated_local_surface_id) {
  DCHECK_GE(child_allocated_local_surface_id.child_sequence_number(),
            last_known_local_surface_id_.child_sequence_number());

  last_known_local_surface_id_.child_sequence_number_ =
      child_allocated_local_surface_id.child_sequence_number_;
  return last_known_local_surface_id_;
}

void ParentLocalSurfaceIdAllocator::Reset(
    const LocalSurfaceId& local_surface_id) {
  last_known_local_surface_id_ = local_surface_id;
}

const LocalSurfaceId& ParentLocalSurfaceIdAllocator::GenerateId() {
  if (!is_allocation_suppressed_)
    ++last_known_local_surface_id_.parent_sequence_number_;
  return last_known_local_surface_id_;
}

}  // namespace viz
