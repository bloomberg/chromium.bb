// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PERFORMANCE_STATS_H_
#define CONTENT_PUBLIC_COMMON_PERFORMANCE_STATS_H_

#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT GpuPerformanceStats {
  GpuPerformanceStats() : graphics(0.f), gaming(0.f), overall(0.f) {}

  float graphics;
  float gaming;
  float overall;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PERFORMANCE_STATS_H_

