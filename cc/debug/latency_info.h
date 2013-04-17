// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_LATENCY_INFO_H_
#define CC_DEBUG_LATENCY_INFO_H_

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "base/time.h"
#include "cc/base/cc_export.h"

namespace cc {

enum LatencyComponentType {
  kRendererMainThread,
  kRendererImplThread,
  kBrowserMainThread,
  kBrowserImplThread,
  kInputEvent,
};

struct CC_EXPORT LatencyInfo {
  struct LatencyComponent {
    // Nondecreasing number that can be used to determine what events happened
    // in the component at the time this struct was sent on to the next
    // component.
    int64 sequence_number;
    // Average time of events that happened in this component.
    base::TimeTicks event_time;
    // Count of events that happened in this component
    uint32 event_count;
  };

  // Map a Latency Component (with a component-specific int64 id) to a
  // component info.
  typedef std::map<std::pair<LatencyComponentType, int64>, LatencyComponent>
      LatencyMap;

  LatencyMap latency_components;

  // This represents the final time that a frame is displayed it.
  base::TimeTicks swap_timestamp;

  LatencyInfo();

  ~LatencyInfo();

  void MergeWith(const LatencyInfo& other);

  void AddLatencyNumber(LatencyComponentType component, int64 id,
                        int64 component_sequence_number);
  void AddLatencyNumberWithTimestamp(LatencyComponentType component,
                                     int64 id, int64 component_sequence_number,
                                     base::TimeTicks time,
                                     uint32 event_count);

  void Clear();
};

}  // namespace cc

#endif  // CC_DEBUG_LATENCY_INFO_H_

