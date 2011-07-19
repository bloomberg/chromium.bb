// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the definition of the IdAllocator class.

#ifndef GPU_COMMAND_BUFFER_CLIENT_ID_ALLOCATOR_H_
#define GPU_COMMAND_BUFFER_CLIENT_ID_ALLOCATOR_H_

#include <set>
#include <utility>
#include "../common/types.h"

namespace gpu {

// A resource ID, key to the resource maps.
typedef uint32 ResourceId;
// Invalid resource ID.
static const ResourceId kInvalidResource = 0u;

// A class to manage the allocation of resource IDs.
class IdAllocator {
 public:
  IdAllocator();
  ~IdAllocator();

  // Allocates a new resource ID.
  ResourceId AllocateID();

  // Allocates an Id starting at or above desired_id.
  // Note: may wrap if it starts near limit.
  ResourceId AllocateIDAtOrAbove(ResourceId desired_id);

  // Marks an id as used. Returns false if id was already used.
  bool MarkAsUsed(ResourceId id);

  // Frees a resource ID.
  void FreeID(ResourceId id);

  // Checks whether or not a resource ID is in use.
  bool InUse(ResourceId id) const;

 private:
  // TODO(gman): This would work much better with ranges or a hash table.
  typedef std::set<ResourceId> ResourceIdSet;

  // The highest ID on the used list.
  ResourceId LastUsedId() const;

  // Lowest ID that isn't on the used list. This is slow, use as a last resort.
  ResourceId FindFirstUnusedId() const;

  ResourceIdSet used_ids_;
  ResourceIdSet free_ids_;

  DISALLOW_COPY_AND_ASSIGN(IdAllocator);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_ID_ALLOCATOR_H_
