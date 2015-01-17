// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/frame_timing_tracker.h"

#include <algorithm>
#include <limits>

#include "base/metrics/histogram.h"
#include "cc/trees/proxy.h"

namespace cc {

FrameTimingTracker::CompositeTimingEvent::CompositeTimingEvent(
    int _frame_id,
    base::TimeTicks _timestamp)
    : frame_id(_frame_id), timestamp(_timestamp) {
}

FrameTimingTracker::CompositeTimingEvent::~CompositeTimingEvent() {
}

// static
scoped_ptr<FrameTimingTracker> FrameTimingTracker::Create() {
  return make_scoped_ptr(new FrameTimingTracker);
}

FrameTimingTracker::FrameTimingTracker() {
}

FrameTimingTracker::~FrameTimingTracker() {
}

void FrameTimingTracker::SaveTimeStamps(
    base::TimeTicks timestamp,
    const std::vector<FrameAndRectIds>& frame_ids) {
  if (!composite_events_)
    composite_events_.reset(new CompositeTimingSet);
  for (const auto& pair : frame_ids)
    (*composite_events_)[pair.second].push_back(
        CompositeTimingEvent(pair.first, timestamp));
}

scoped_ptr<FrameTimingTracker::CompositeTimingSet>
FrameTimingTracker::GroupCountsByRectId() {
  if (!composite_events_)
    return make_scoped_ptr(new CompositeTimingSet);
  scoped_ptr<CompositeTimingSet> composite_info(new CompositeTimingSet);
  for (auto& infos : *composite_events_)
    std::sort(
        infos.second.begin(), infos.second.end(),
        [](const CompositeTimingEvent& lhs, const CompositeTimingEvent& rhs) {
          return lhs.timestamp < rhs.timestamp;
        });
  return composite_events_.Pass();
}

}  // namespace cc
