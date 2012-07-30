// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERING_BENCHMARK_RESULTS_H_
#define CONTENT_RENDERER_RENDERING_BENCHMARK_RESULTS_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

class RenderingBenchmarkResults {
 public:
  virtual ~RenderingBenchmarkResults() { }

  virtual void AddResult(const std::string& benchmark_name,
                         const std::string& result_name,
                         const std::string& result_unit,
                         double time) = 0;
};
}  // namespace content

#endif  // CONTENT_RENDERER_RENDERING_BENCHMARK_RESULTS_H_
