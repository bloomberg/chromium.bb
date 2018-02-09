// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

#include "base/rand_util.h"

namespace viz {

namespace {
constexpr uint32_t kInvalidParentSequenceNumber = 0;
constexpr uint32_t kInitialChildSequenceNumber = 1;
}  // namespace

ParentLocalSurfaceIdAllocator::ParentLocalSurfaceIdAllocator()
    : last_known_local_surface_id_(kInvalidParentSequenceNumber,
                                   kInitialChildSequenceNumber,
                                   base::UnguessableToken()) {}

const LocalSurfaceId& ParentLocalSurfaceIdAllocator::UpdateFromChild(
    const LocalSurfaceId& child_allocated_local_surface_id) {
  DCHECK_GE(child_allocated_local_surface_id.child_sequence_number(),
            last_known_local_surface_id_.child_sequence_number());

  last_known_local_surface_id_ =
      LocalSurfaceId(last_known_local_surface_id_.parent_sequence_number(),
                     child_allocated_local_surface_id.child_sequence_number(),
                     last_known_local_surface_id_.nonce());
  return last_known_local_surface_id_;
}

const LocalSurfaceId& ParentLocalSurfaceIdAllocator::GenerateId() {
  last_known_local_surface_id_ =
      LocalSurfaceId(last_known_local_surface_id_.parent_sequence_number() + 1,
                     last_known_local_surface_id_.child_sequence_number(),
                     base::UnguessableToken::Create());
  return last_known_local_surface_id_;
}

}  // namespace viz
