// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "managed_memory_policy.h"

#include "priority_calculator.h"

namespace cc {

ManagedMemoryPolicy::ManagedMemoryPolicy(size_t bytesLimitWhenVisible)
    : bytesLimitWhenVisible(bytesLimitWhenVisible)
    , priorityCutoffWhenVisible(CUTOFF_ALLOW_EVERYTHING)
    , bytesLimitWhenNotVisible(0)
    , priorityCutoffWhenNotVisible(CUTOFF_ALLOW_NOTHING)
{
}

ManagedMemoryPolicy::ManagedMemoryPolicy(size_t bytesLimitWhenVisible,
                                         PriorityCutoff priorityCutoffWhenVisible,
                                         size_t bytesLimitWhenNotVisible,
                                         PriorityCutoff priorityCutoffWhenNotVisible)
    : bytesLimitWhenVisible(bytesLimitWhenVisible)
    , priorityCutoffWhenVisible(priorityCutoffWhenVisible)
    , bytesLimitWhenNotVisible(bytesLimitWhenNotVisible)
    , priorityCutoffWhenNotVisible(priorityCutoffWhenNotVisible)
{
}

bool ManagedMemoryPolicy::operator==(const ManagedMemoryPolicy& other) const
{
    return bytesLimitWhenVisible == other.bytesLimitWhenVisible &&
           priorityCutoffWhenVisible == other.priorityCutoffWhenVisible &&
           bytesLimitWhenNotVisible == other.bytesLimitWhenNotVisible &&
           priorityCutoffWhenNotVisible == other.priorityCutoffWhenNotVisible;
}

bool ManagedMemoryPolicy::operator!=(const ManagedMemoryPolicy& other) const
{
    return !(*this == other);
}

// static
int ManagedMemoryPolicy::priorityCutoffToValue(PriorityCutoff priorityCutoff)
{
    switch (priorityCutoff) {
    case CUTOFF_ALLOW_NOTHING:
        return PriorityCalculator::allowNothingCutoff();
    case CUTOFF_ALLOW_REQUIRED_ONLY:
        return PriorityCalculator::allowVisibleOnlyCutoff();
    case CUTOFF_ALLOW_NICE_TO_HAVE:
        return PriorityCalculator::allowVisibleAndNearbyCutoff();
    case CUTOFF_ALLOW_EVERYTHING:
        return PriorityCalculator::allowEverythingCutoff();
    }
    NOTREACHED();
    return PriorityCalculator::allowNothingCutoff();
}

// static
TileMemoryLimitPolicy ManagedMemoryPolicy::priorityCutoffToTileMemoryLimitPolicy(PriorityCutoff priorityCutoff)
{
    switch (priorityCutoff) {
    case CUTOFF_ALLOW_NOTHING:
        return ALLOW_NOTHING;
    case CUTOFF_ALLOW_REQUIRED_ONLY:
        return ALLOW_ABSOLUTE_MINIMUM;
    case CUTOFF_ALLOW_NICE_TO_HAVE:
        return ALLOW_PREPAINT_ONLY;
    case CUTOFF_ALLOW_EVERYTHING:
        return ALLOW_ANYTHING;
    }
    NOTREACHED();
    return ALLOW_NOTHING;
}

}  // namespace cc
