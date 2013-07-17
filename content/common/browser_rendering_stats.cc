// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/browser_rendering_stats.h"

namespace content {

BrowserRenderingStats::BrowserRenderingStats() :
    input_event_count(0),
    touch_ui_count(0),
    touch_acked_count(0),
    scroll_update_count(0) {
}

BrowserRenderingStats::~BrowserRenderingStats() {}

void BrowserRenderingStats::EnumerateFields(
    cc::RenderingStats::Enumerator* enumerator) const {
  enumerator->AddInt("inputEventCount", input_event_count);
  enumerator->AddTimeDeltaInSecondsF("totalInputLatency", total_input_latency);

  enumerator->AddInt("touchUICount", touch_ui_count);
  enumerator->AddTimeDeltaInSecondsF("totalTouchUILatency",
                                     total_touch_ui_latency);

  enumerator->AddInt("touchAckedCount", touch_acked_count);
  enumerator->AddTimeDeltaInSecondsF("totalTouchAckedLatency",
                                     total_touch_acked_latency);

  enumerator->AddInt("scrollUpdateCount", scroll_update_count);
  enumerator->AddTimeDeltaInSecondsF("totalScrollUpdateLatency",
                                     total_scroll_update_latency);
}

}  // namespace content
