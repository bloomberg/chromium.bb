// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERING_BENCHMARK_H_
#define CONTENT_RENDERER_RENDERING_BENCHMARK_H_

#include <string>

#include "base/basictypes.h"

namespace WebKit {
class WebViewBenchmarkSupport;
}

namespace content {
class RenderingBenchmarkResults;

class RenderingBenchmark {
 public:
  explicit RenderingBenchmark(const std::string& name);
  virtual ~RenderingBenchmark();

  virtual void SetUp(WebKit::WebViewBenchmarkSupport* benchmarkSupport) {}

  virtual double Run(WebKit::WebViewBenchmarkSupport* benchmarkSupport) = 0;

  virtual void TearDown(WebKit::WebViewBenchmarkSupport* benchmarkSupport) {}

  const std::string& name() { return name_; }

 private:
  const std::string name_;

  DISALLOW_COPY_AND_ASSIGN(RenderingBenchmark);
};
}  // namespace content

#endif  // CONTENT_RENDERER_RENDERING_BENCHMARK_H_
