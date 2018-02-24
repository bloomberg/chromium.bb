// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"

#include <stdint.h>

#include "base/rand_util.h"

namespace viz {

ChildLocalSurfaceIdAllocator::ChildLocalSurfaceIdAllocator()
    : last_known_local_surface_id_(kInvalidParentSequenceNumber,
                                   kInitialChildSequenceNumber,
                                   base::UnguessableToken()) {}

const LocalSurfaceId& ChildLocalSurfaceIdAllocator::UpdateFromParent(
    const LocalSurfaceId& parent_allocated_local_surface_id) {
  DCHECK_GE(parent_allocated_local_surface_id.parent_sequence_number(),
            last_known_local_surface_id_.parent_sequence_number());
  // Thie verifies that we only update the nonce if the parent sequence number
  // has changed.
  DCHECK(parent_allocated_local_surface_id.parent_sequence_number() >
             last_known_local_surface_id_.parent_sequence_number() ||
         parent_allocated_local_surface_id.nonce() ==
             last_known_local_surface_id_.nonce());

  last_known_local_surface_id_.parent_sequence_number_ =
      parent_allocated_local_surface_id.parent_sequence_number_;
  last_known_local_surface_id_.nonce_ =
      parent_allocated_local_surface_id.nonce_;
  return last_known_local_surface_id_;
}

const LocalSurfaceId& ChildLocalSurfaceIdAllocator::GenerateId() {
  // UpdateFromParent must be called before we can generate a valid ID.
  DCHECK_NE(last_known_local_surface_id_.parent_sequence_number(),
            kInvalidParentSequenceNumber);

  ++last_known_local_surface_id_.child_sequence_number_;
  return last_known_local_surface_id_;
}

}  // namespace viz
