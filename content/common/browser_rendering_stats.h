// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_BROWSER_RENDERING_STATS_H_
#define CONTENT_COMMON_BROWSER_RENDERING_STATS_H_

#include "base/time.h"
#include "cc/debug/rendering_stats.h"
#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT BrowserRenderingStats {
  BrowserRenderingStats();
  ~BrowserRenderingStats();

  uint32 input_event_count;
  base::TimeDelta total_input_latency;

  // Note: when adding new members, please remember to update enumerateFields
  // in browser_rendering_stats.cc.

  // Outputs the fields in this structure to the provided enumerator.
  void EnumerateFields(cc::RenderingStats::Enumerator* enumerator) const;
};

}  // namespace content

#endif  // CONTENT_COMMON_BROWSER_RENDERING_STATS_H_
