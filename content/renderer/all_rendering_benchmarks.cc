// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/all_rendering_benchmarks.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "content/renderer/rendering_benchmark.h"
#include "content/renderer/rendering_benchmark_results.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewBenchmarkSupport.h"

using base::TimeDelta;
using base::TimeTicks;
using WebKit::WebSize;
using WebKit::WebCanvas;
using WebKit::WebViewBenchmarkSupport;
using WebKit::WebRect;
using std::vector;

namespace {

// This is a base class for timing the painting of the current webpage to
// custom WebCanvases.
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
    before_time_ = TimeTicks::HighResNow();
    return canvas;
  }

  virtual void didPaint(WebCanvas* canvas) OVERRIDE {
    paint_time_total_ += (TimeTicks::HighResNow() - before_time_);
    delete canvas;
  }

  virtual void Run(content::RenderingBenchmarkResults* results,
                   WebViewBenchmarkSupport* support) OVERRIDE {
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
    return skia::CreateBitmapCanvas(size.width, size.height, false);
  }
};

class NullCanvasPaintBenchmark : public CustomPaintBenchmark {
 public:
  NullCanvasPaintBenchmark(const std::string& name,
                           WebViewBenchmarkSupport::PaintMode paint_mode)
      : CustomPaintBenchmark(name, paint_mode),
        canvas_count_(0) { }

  virtual void Run(content::RenderingBenchmarkResults* results,
                   WebViewBenchmarkSupport* support) OVERRIDE {
    canvas_count_ = 0;
    CustomPaintBenchmark::Run(results, support);
    results->AddResult(name(),
                       "canvasCount",
                       "i",
                       canvas_count_);
  }

 private:
  virtual WebCanvas* createCanvas(const WebSize& size) OVERRIDE {
    ++canvas_count_;
    return SkCreateNullCanvas();
  }

  int canvas_count_;
};

class SkPicturePaintBenchmark : public CustomPaintBenchmark {
 public:
  SkPicturePaintBenchmark(const std::string& name,
                          WebViewBenchmarkSupport::PaintMode paint_mode)
      : CustomPaintBenchmark(name, paint_mode) { }

  virtual void didPaint(WebCanvas* canvas) OVERRIDE {
    DCHECK(picture_.getRecordingCanvas() == canvas);
    picture_.endRecording();
    CustomPaintBenchmark::didPaint(NULL);
  }

 private:
  virtual WebCanvas* createCanvas(const WebSize& size) OVERRIDE {
    return picture_.beginRecording(size.width, size.height);
  }

  SkPicture picture_;
};

// Base class for timing the replaying of the SkPicture into canvases.
class TiledReplayBenchmark
    : public content::RenderingBenchmark,
      public WebViewBenchmarkSupport::PaintClient {
 public:
  TiledReplayBenchmark(const std::string& name,
                       WebViewBenchmarkSupport::PaintMode paint_mode)
      : RenderingBenchmark(name),
        paint_mode_(paint_mode) {}

  virtual WebCanvas* willPaint(const WebSize& size) OVERRIDE {
    return picture_.beginRecording(size.width, size.height);
  }

  virtual void didPaint(WebCanvas* canvas) OVERRIDE {
    DCHECK(picture_.getRecordingCanvas() == canvas);
    picture_.endRecording();

    const vector<WebRect> repaint_tiles = GetRepaintTiles(
        WebSize(picture_.width(), picture_.height()));

    vector<WebRect>::const_iterator it;
    for (it = repaint_tiles.begin(); it != repaint_tiles.end(); ++it) {
      WebRect tile = *it;
      scoped_ptr<WebCanvas> canvas(
          skia::CreateBitmapCanvas(tile.width, tile.height, false));
      TimeTicks before_time = TimeTicks::HighResNow();
      canvas->translate(-tile.x, -tile.y);
      picture_.draw(canvas.get());
      paint_time_total_ += (TimeTicks::HighResNow() - before_time);
    }
  }

  virtual void Run(content::RenderingBenchmarkResults* results,
                   WebViewBenchmarkSupport* support) {
    paint_time_total_ = TimeDelta();
    support->paint(this, paint_mode_);
    results->AddResult(name(),
                       "repaintTime",
                       "s",
                       paint_time_total_.InSecondsF());
  }

 private:
  virtual vector<WebRect> GetRepaintTiles(const WebSize& layer_size) const = 0;

  TimeDelta paint_time_total_;
  SkPicture picture_;
  const WebViewBenchmarkSupport::PaintMode paint_mode_;
};

class SquareTiledReplayBenchmark : public TiledReplayBenchmark {
 public:
  SquareTiledReplayBenchmark(const std::string& name,
                             WebViewBenchmarkSupport::PaintMode paint_mode,
                             int tile_size)
      : TiledReplayBenchmark(name, paint_mode),
        tile_size_(tile_size) {
    CHECK_GT(tile_size, 0);
  }

 private:
  virtual vector<WebRect> GetRepaintTiles(
      const WebSize& layer_size) const OVERRIDE {
    vector<WebRect> tiles;
    for (int x = 0; x < layer_size.width; x += tile_size_) {
      for (int y = 0; y < layer_size.height; y += tile_size_) {
        int width = std::min(layer_size.width - x, tile_size_);
        int height = std::min(layer_size.height - y, tile_size_);
        tiles.push_back(WebRect(x, y, width, height));
      }
    }
    return tiles;
  }

  int tile_size_;
};

class LayerWidthTiledReplayBenchmark : public TiledReplayBenchmark {
 public:
  LayerWidthTiledReplayBenchmark(const std::string& name,
                                 WebViewBenchmarkSupport::PaintMode paint_mode,
                                 int tile_height)
      : TiledReplayBenchmark(name, paint_mode),
        tile_height_(tile_height) {
    CHECK_GT(tile_height, 0);
  }

 private:
  virtual vector<WebRect> GetRepaintTiles(
      const WebSize& layer_size) const OVERRIDE {
    vector<WebRect> tiles;
    for (int y = 0; y < layer_size.height; y += tile_height_) {
      int height = std::min(layer_size.height - y, tile_height_);
      tiles.push_back(WebRect(0, y, layer_size.width, height));
    }
    return tiles;
  }

  int tile_height_;
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
  benchmarks.push_back(new SkPicturePaintBenchmark(
      "PaintEverythingToSkPicture",
      WebViewBenchmarkSupport::PaintModeEverything));
  benchmarks.push_back(new SquareTiledReplayBenchmark(
      "RepaintEverythingTo256x256Bitmap",
      WebViewBenchmarkSupport::PaintModeEverything,
      256));
  benchmarks.push_back(new SquareTiledReplayBenchmark(
      "RepaintEverythingTo128x128Bitmap",
      WebViewBenchmarkSupport::PaintModeEverything,
      128));
  benchmarks.push_back(new SquareTiledReplayBenchmark(
      "RepaintEverythingTo512x512Bitmap",
      WebViewBenchmarkSupport::PaintModeEverything,
      512));
  benchmarks.push_back(new LayerWidthTiledReplayBenchmark(
      "RepaintEverythingToLayerWidthx256Bitmap",
      WebViewBenchmarkSupport::PaintModeEverything,
      256));
  benchmarks.push_back(new LayerWidthTiledReplayBenchmark(
      "RepaintEverythingToLayerWidthx128Bitmap",
      WebViewBenchmarkSupport::PaintModeEverything,
      128));
  benchmarks.push_back(new LayerWidthTiledReplayBenchmark(
      "RepaintEverythingToLayerWidthx64Bitmap",
      WebViewBenchmarkSupport::PaintModeEverything,
      64));
  benchmarks.push_back(new LayerWidthTiledReplayBenchmark(
      "RepaintEverythingToLayerWidthx512Bitmap",
      WebViewBenchmarkSupport::PaintModeEverything,
      512));
  return benchmarks.Pass();
}

}  // namespace content
