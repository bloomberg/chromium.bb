// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/helium/vector_clock.h"

#include <algorithm>

#include "base/logging.h"

namespace blimp {

VectorClock::VectorClock() {}

VectorClock::VectorClock(Revision local_revision, Revision remote_revision)
    : local_revision_(local_revision), remote_revision_(remote_revision) {}

VectorClock::Comparison VectorClock::CompareTo(const VectorClock& other) const {
  DCHECK(local_revision_ >= other.local_revision());

  if (local_revision_ == other.local_revision()) {
    if (remote_revision_ == other.remote_revision()) {
      return VectorClock::Comparison::EqualTo;
    } else if (remote_revision_ < other.remote_revision()) {
      return VectorClock::Comparison::LessThan;
    } else {
      return VectorClock::Comparison::GreaterThan;
    }
  } else {
    if (local_revision_ > other.local_revision()) {
      if (remote_revision_ == other.remote_revision()) {
        return VectorClock::Comparison::GreaterThan;
      } else {
        return VectorClock::Comparison::Conflict;
      }
    } else {  // We know its not equal or greater, so its smaller
      if (remote_revision_ == other.remote_revision()) {
        return VectorClock::Comparison::LessThan;
      } else {
        LOG(FATAL) << "Local revision should always be greater or equal.";
        return VectorClock::Comparison::Conflict;
      }
    }
  }
}

VectorClock VectorClock::MergeWith(const VectorClock& other) const {
  VectorClock result(std::max(local_revision_, other.local_revision()),
                     std::max(remote_revision_, other.remote_revision()));
  return result;
}

void VectorClock::IncrementLocal() {
  local_revision_++;
}

}  // namespace blimp
