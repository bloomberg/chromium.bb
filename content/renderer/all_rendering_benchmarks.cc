// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/all_rendering_benchmarks.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "content/renderer/rendering_benchmark.h"
#include "content/renderer/rendering_benchmark_results.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCanvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewBenchmarkSupport.h"

using base::TimeDelta;
using base::TimeTicks;
using WebKit::WebSize;
using WebKit::WebCanvas;
using WebKit::WebViewBenchmarkSupport;

namespace {

class CustomPaintBenchmark
    : public content::RenderingBenchmark,
      public WebViewBenchmarkSupport::PaintClient {
 public:
  CustomPaintBenchmark(const std::string& name,
                       WebViewBenchmarkSupport::PaintMode paint_mode)
      : content::RenderingBenchmark(name),
        paint_mode_(paint_mode) { }

  virtual WebCanvas* willPaint(const WebSize& size) OVERRIDE {
    WebCanvas* canvas = createCanvas(size);
    before_time_ = TimeTicks::Now();
    return canvas;
  }

  virtual void didPaint(WebCanvas* canvas) OVERRIDE {
    paint_time_total_ += (TimeTicks::Now() - before_time_);
    delete canvas;
  }

  virtual void Run(content::RenderingBenchmarkResults* results,
                   WebViewBenchmarkSupport* support) {
    paint_time_total_ = TimeDelta();
    support->paint(this, paint_mode_);
    results->AddResult(name(),
                       "paintTime",
                       "s",
                       paint_time_total_.InSecondsF());
  }

 private:
  virtual WebCanvas* createCanvas(const WebSize& size) = 0;

  TimeTicks before_time_;
  TimeDelta paint_time_total_;
  const WebViewBenchmarkSupport::PaintMode paint_mode_;
};

class BitmapCanvasPaintBenchmark : public CustomPaintBenchmark {
 public:
  BitmapCanvasPaintBenchmark(const std::string& name,
                             WebViewBenchmarkSupport::PaintMode paint_mode)
      : CustomPaintBenchmark(name, paint_mode) { }

 private:
  virtual WebCanvas* createCanvas(const WebSize& size) OVERRIDE {
    SkDevice* device = new SkDevice(SkBitmap::kARGB_8888_Config,
                                    size.width,
                                    size.height,
                                    false);
    WebCanvas* canvas = new WebCanvas(device);
    device->unref();
    return canvas;
  }
};

class NullCanvasPaintBenchmark : public CustomPaintBenchmark {
 public:
  NullCanvasPaintBenchmark(const std::string& name,
                           WebViewBenchmarkSupport::PaintMode paint_mode)
      : CustomPaintBenchmark(name, paint_mode) { }

 private:
  virtual WebCanvas* createCanvas(const WebSize& size) OVERRIDE {
    return SkCreateNullCanvas();
  }
};
}  // anonymous namespace

namespace content {

ScopedVector<RenderingBenchmark> AllRenderingBenchmarks() {
  ScopedVector<RenderingBenchmark> benchmarks;
  benchmarks.push_back(new BitmapCanvasPaintBenchmark(
      "PaintEverythingToBitmap",
      WebViewBenchmarkSupport::PaintModeEverything));
  benchmarks.push_back(new NullCanvasPaintBenchmark(
      "PaintEverythingToNullCanvas",
      WebViewBenchmarkSupport::PaintModeEverything));
  return benchmarks.Pass();
}

}  // namespace content
