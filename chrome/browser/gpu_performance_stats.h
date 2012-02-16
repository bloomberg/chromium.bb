// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_PERFORMANCE_STATS_H_
#define CHROME_BROWSER_GPU_PERFORMANCE_STATS_H_

#include "base/values.h"

class GpuPerformanceStats {
 public:
  GpuPerformanceStats() : graphics(0.f), gaming(0.f), overall(0.f) {
  }

  static GpuPerformanceStats RetrieveGpuPerformanceStats();

  base::Value* ToValue() const;

  float graphics;
  float gaming;
  float overall;
};


#endif  // CHROME_BROWSER_GPU_PERFORMANCE_STATS_H_
