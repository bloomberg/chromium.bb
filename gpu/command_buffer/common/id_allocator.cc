// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of IdAllocator.

#include "../common/id_allocator.h"
#include "../common/logging.h"

namespace gpu {

IdAllocator::IdAllocator() {
}

ResourceId IdAllocator::FindFirstFree() const {
  ResourceId id = 1;
  for (ResourceIdSet::const_iterator it = used_ids_.begin();
       it != used_ids_.end(); ++it) {
    if ((*it) != id) {
      return id;
    }
    ++id;
  }
  return id;
}

ResourceId IdAllocator::AllocateIDAtOrAbove(ResourceId desired_id) {
  GPU_DCHECK_LT(static_cast<ResourceId>(used_ids_.size()),
            static_cast<ResourceId>(-1));
  for (; InUse(desired_id); ++desired_id) {}
  MarkAsUsed(desired_id);
  return desired_id;
}

}  // namespace gpu
