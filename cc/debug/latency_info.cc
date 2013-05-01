// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/latency_info.h"

#include <algorithm>

namespace cc {

LatencyInfo::LatencyInfo() {
}

LatencyInfo::~LatencyInfo() {
}

void LatencyInfo::MergeWith(const LatencyInfo& other) {
  for (LatencyMap::const_iterator b = other.latency_components.begin();
      b != other.latency_components.end(); ++b) {
    AddLatencyNumberWithTimestamp(b->first.first, b->first.second,
                                  b->second.sequence_number,
                                  b->second.event_time,
                                  b->second.event_count);
  }
}

void LatencyInfo::AddLatencyNumber(LatencyComponentType component,
                                   int64 id, int64 component_sequence_number) {
  AddLatencyNumberWithTimestamp(component, id, component_sequence_number,
                                base::TimeTicks::Now(), 1);
}

void LatencyInfo::AddLatencyNumberWithTimestamp(
    LatencyComponentType component, int64 id, int64 component_sequence_number,
    base::TimeTicks time, uint32 event_count) {
  LatencyMap::key_type key = std::make_pair(component, id);
  LatencyMap::iterator f = latency_components.find(key);
  if (f == latency_components.end()) {
    LatencyComponent info = {component_sequence_number, time, event_count};
    latency_components[key] = info;
  } else {
    f->second.sequence_number = std::max(component_sequence_number,
                                         f->second.sequence_number);
    uint32 new_count = event_count + f->second.event_count;
    if (event_count > 0 && new_count != 0) {
      // Do a weighted average, so that the new event_time is the average of
      // the times of events currently in this structure with the time passed
      // into this method.
      f->second.event_time += (time - f->second.event_time) * event_count /
          new_count;
      f->second.event_count = new_count;
    }
  }
}

void LatencyInfo::Clear() {
  latency_components.clear();
}

}  // namespace cc

