// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/managed_memory_policy.h"

#include "base/logging.h"
#include "cc/resources/priority_calculator.h"

namespace cc {

ManagedMemoryPolicy::ManagedMemoryPolicy(size_t bytes_limit_when_visible)
    : bytes_limit_when_visible(bytes_limit_when_visible),
      priority_cutoff_when_visible(CUTOFF_ALLOW_EVERYTHING),
      bytes_limit_when_not_visible(0),
      priority_cutoff_when_not_visible(CUTOFF_ALLOW_NOTHING) {}

ManagedMemoryPolicy::ManagedMemoryPolicy(
    size_t bytes_limit_when_visible,
    PriorityCutoff priority_cutoff_when_visible,
    size_t bytes_limit_when_not_visible,
    PriorityCutoff priority_cutoff_when_not_visible)
    : bytes_limit_when_visible(bytes_limit_when_visible),
      priority_cutoff_when_visible(priority_cutoff_when_visible),
      bytes_limit_when_not_visible(bytes_limit_when_not_visible),
      priority_cutoff_when_not_visible(priority_cutoff_when_not_visible) {}

bool ManagedMemoryPolicy::operator==(const ManagedMemoryPolicy& other) const {
  return bytes_limit_when_visible == other.bytes_limit_when_visible &&
         priority_cutoff_when_visible == other.priority_cutoff_when_visible &&
         bytes_limit_when_not_visible == other.bytes_limit_when_not_visible &&
         priority_cutoff_when_not_visible ==
         other.priority_cutoff_when_not_visible;
}

bool ManagedMemoryPolicy::operator!=(const ManagedMemoryPolicy& other) const {
  return !(*this == other);
}

// static
int ManagedMemoryPolicy::PriorityCutoffToValue(PriorityCutoff priority_cutoff) {
  switch (priority_cutoff) {
    case CUTOFF_ALLOW_NOTHING:
      return PriorityCalculator::AllowNothingCutoff();
    case CUTOFF_ALLOW_REQUIRED_ONLY:
      return PriorityCalculator::AllowVisibleOnlyCutoff();
    case CUTOFF_ALLOW_NICE_TO_HAVE:
      return PriorityCalculator::AllowVisibleAndNearbyCutoff();
    case CUTOFF_ALLOW_EVERYTHING:
      return PriorityCalculator::AllowEverythingCutoff();
  }
  NOTREACHED();
  return PriorityCalculator::AllowNothingCutoff();
}

// static
TileMemoryLimitPolicy
ManagedMemoryPolicy::PriorityCutoffToTileMemoryLimitPolicy(
    PriorityCutoff priority_cutoff) {
  switch (priority_cutoff) {
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
