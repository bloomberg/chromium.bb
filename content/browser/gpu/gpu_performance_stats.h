// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_PERFORMANCE_STATS_H_
#define CONTENT_BROWSER_GPU_GPU_PERFORMANCE_STATS_H_

#include "base/values.h"
#include "content/common/content_export.h"

class CONTENT_EXPORT GpuPerformanceStats {
 public:
  GpuPerformanceStats() : graphics(0.f), gaming(0.f), overall(0.f) {
  }

  static GpuPerformanceStats RetrieveGpuPerformanceStats();

  base::Value* ToValue() const;

  float graphics;
  float gaming;
  float overall;
};


#endif  // CONTENT_BROWSER_GPU_GPU_PERFORMANCE_STATS_H_
