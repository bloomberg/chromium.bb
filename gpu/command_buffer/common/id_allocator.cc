// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of IdAllocator.

#include "../common/id_allocator.h"
#include "../common/logging.h"

namespace gpu {

IdAllocator::IdAllocator() {}

IdAllocator::~IdAllocator() {}

ResourceId IdAllocator::AllocateID() {
  ResourceId id = FindFirstFree();
  MarkAsUsed(id);
  return id;
}

ResourceId IdAllocator::AllocateIDAtOrAbove(ResourceId desired_id) {
  GPU_DCHECK_LT(static_cast<ResourceId>(used_ids_.size()),
            static_cast<ResourceId>(-1));
  for (; InUse(desired_id); ++desired_id) {}
  MarkAsUsed(desired_id);
  return desired_id;
}

bool IdAllocator::MarkAsUsed(ResourceId id) {
  std::pair<ResourceIdSet::iterator, bool> result = used_ids_.insert(id);
  return result.second;
}

void IdAllocator::FreeID(ResourceId id) {
  used_ids_.erase(id);
}

bool IdAllocator::InUse(ResourceId id) const {
  return id == kInvalidResource || used_ids_.find(id) != used_ids_.end();
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

}  // namespace gpu
