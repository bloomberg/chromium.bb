// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the definition of the IdAllocator class.

#ifndef GPU_COMMAND_BUFFER_CLIENT_ID_ALLOCATOR_H_
#define GPU_COMMAND_BUFFER_CLIENT_ID_ALLOCATOR_H_

#include <vector>
#include "base/basictypes.h"
#include "gpu/command_buffer/common/types.h"
#include "gpu/command_buffer/common/resource.h"

namespace gpu {

// A class to manage the allocation of resource IDs. It uses a bitfield stored
// into a vector of unsigned ints.
class IdAllocator {
 public:
  IdAllocator();

  // Allocates a new resource ID.
  ResourceId AllocateID() {
    unsigned int bit = FindFirstFree();
    SetBit(bit, true);
    return bit;
  }

  // Frees a resource ID.
  void FreeID(ResourceId id) {
    SetBit(id, false);
  }

  // Checks whether or not a resource ID is in use.
  bool InUse(ResourceId id) {
    return GetBit(id);
  }
 private:
  void SetBit(unsigned int bit, bool value);
  bool GetBit(unsigned int bit) const;
  unsigned int FindFirstFree() const;

  std::vector<Uint32> bitmap_;
  DISALLOW_COPY_AND_ASSIGN(IdAllocator);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_ID_ALLOCATOR_H_
