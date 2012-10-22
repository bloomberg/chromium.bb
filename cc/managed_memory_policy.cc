// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "managed_memory_policy.h"

#include "priority_calculator.h"

namespace cc {

ManagedMemoryPolicy::ManagedMemoryPolicy(size_t bytesLimitWhenVisible)
    : bytesLimitWhenVisible(bytesLimitWhenVisible)
    , priorityCutoffWhenVisible(CCPriorityCalculator::allowEverythingCutoff())
    , bytesLimitWhenNotVisible(0)
    , priorityCutoffWhenNotVisible(CCPriorityCalculator::allowNothingCutoff())
{
}

ManagedMemoryPolicy::ManagedMemoryPolicy(size_t bytesLimitWhenVisible,
                                         int priorityCutoffWhenVisible,
                                         size_t bytesLimitWhenNotVisible,
                                         int priorityCutoffWhenNotVisible)
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

} // namespace cc
