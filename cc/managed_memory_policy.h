// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MANAGED_MEMORY_POLICY_H_
#define CC_MANAGED_MEMORY_POLICY_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "cc/tile_priority.h"

namespace cc {

struct CC_EXPORT ManagedMemoryPolicy {
     enum PriorityCutoff {
        CUTOFF_ALLOW_NOTHING,
        CUTOFF_ALLOW_REQUIRED_ONLY,
        CUTOFF_ALLOW_NICE_TO_HAVE,
        CUTOFF_ALLOW_EVERYTHING,
    };

    ManagedMemoryPolicy(size_t bytesLimitWhenVisible);
    ManagedMemoryPolicy(size_t bytesLimitWhenVisible,
                        PriorityCutoff priorityCutoffWhenVisible,
                        size_t bytesLimitWhenNotVisible,
                        PriorityCutoff priorityCutoffWhenNotVisible);
    bool operator==(const ManagedMemoryPolicy&) const;
    bool operator!=(const ManagedMemoryPolicy&) const;

    size_t bytesLimitWhenVisible;
    PriorityCutoff priorityCutoffWhenVisible;
    size_t bytesLimitWhenNotVisible;
    PriorityCutoff priorityCutoffWhenNotVisible;

    static int priorityCutoffToValue(PriorityCutoff priorityCutoff);
    static TileMemoryLimitPolicy priorityCutoffToTileMemoryLimitPolicy(PriorityCutoff priorityCutoff);
};

}  // namespace cc

#endif  // CC_MANAGED_MEMORY_POLICY_H_
