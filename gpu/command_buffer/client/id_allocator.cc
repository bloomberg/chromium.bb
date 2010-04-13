// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of IdAllocator.

#include "../client/id_allocator.h"
#include "../common/logging.h"

namespace gpu {

IdAllocator::IdAllocator() {
}

unsigned int IdAllocator::FindFirstFree() const {
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
