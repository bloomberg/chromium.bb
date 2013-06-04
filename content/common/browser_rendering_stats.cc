// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/browser_rendering_stats.h"

namespace content {

BrowserRenderingStats::BrowserRenderingStats() : input_event_count(0) {}

BrowserRenderingStats::~BrowserRenderingStats() {}

void BrowserRenderingStats::EnumerateFields(
    cc::RenderingStats::Enumerator* enumerator) const {
  enumerator->AddInt("inputEventCount", input_event_count);
  enumerator->AddTimeDeltaInSecondsF("totalInputLatency", total_input_latency);
}

}  // namespace content
