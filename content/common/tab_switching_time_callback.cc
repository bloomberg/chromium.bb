// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/tab_switching_time_callback.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"

namespace content {

base::OnceCallback<void(base::TimeTicks, base::TimeDelta, uint32_t)>
CreateTabSwitchingTimeRecorder(base::TimeTicks request_time) {
  static uint32_t trace_id = 0;
  TRACE_EVENT_ASYNC_BEGIN0("latency", "TabSwitching::Latency",
                           TRACE_ID_LOCAL(trace_id));
  return base::BindOnce(
      [](base::TimeTicks request_timestamp, uint32_t trace_id,
         base::TimeTicks present_timestamp, base::TimeDelta interval,
         uint32_t flags) {
        const auto delta = present_timestamp - request_timestamp;
        UMA_HISTOGRAM_TIMES("MPArch.RWH_TabSwitchPaintDuration", delta);
        TRACE_EVENT_ASYNC_END1("latency", "TabSwitching::Latency",
                               TRACE_ID_LOCAL(trace_id), "latency",
                               delta.InMillisecondsF());
      },
      request_time, trace_id);
  ++trace_id;
}

}  // namespace content
