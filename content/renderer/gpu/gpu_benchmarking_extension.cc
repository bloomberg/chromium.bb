// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_benchmarking_extension.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_vector.h"
#include "base/string_number_conversions.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/all_rendering_benchmarks.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/rendering_benchmark.h"
#include "content/renderer/rendering_benchmark_results.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRenderingStats.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewBenchmarkSupport.h"
#include "v8/include/v8.h"

using WebKit::WebCanvas;
using WebKit::WebFrame;
using WebKit::WebPrivatePtr;
using WebKit::WebRenderingStats;
using WebKit::WebSize;
using WebKit::WebView;
using WebKit::WebViewBenchmarkSupport;

const char kGpuBenchmarkingExtensionName[] = "v8/GpuBenchmarking";

namespace {

// Always called on the main render thread.
// Does not need to be thread-safe.
void InitSkGraphics() {
  static bool init = false;
  if (!init) {
    SkGraphics::Init();
    init = true;
  }
}

class SkPictureRecorder : public WebViewBenchmarkSupport::PaintClient {
 public:
  explicit SkPictureRecorder(const FilePath& dirpath)
      : dirpath_(dirpath),
        layer_id_(0) {
    // Let skia register known effect subclasses. This basically enables
    // reflection on those subclasses required for picture serialization.
    InitSkGraphics();
  }

  virtual WebCanvas* willPaint(const WebSize& size) {
    return picture_.beginRecording(size.width, size.height);
  }

  virtual void didPaint(WebCanvas* canvas) {
    DCHECK(canvas == picture_.getRecordingCanvas());
    picture_.endRecording();
    // Serialize picture to file.
    // TODO(alokp): Note that for this to work Chrome needs to be launched with
    // --no-sandbox command-line flag. Get rid of this limitation.
    // CRBUG: 139640.
    std::string filename = "layer_" + base::IntToString(layer_id_++) + ".skp";
    std::string filepath = dirpath_.AppendASCII(filename).MaybeAsASCII();
    DCHECK(!filepath.empty());
    SkFILEWStream file(filepath.c_str());
    DCHECK(file.isValid());
    picture_.serialize(&file);
  }

 private:
  FilePath dirpath_;
  int layer_id_;
  SkPicture picture_;
};

}  // namespace

namespace content {

// Benchmark results object that populates a v8 array.
class V8BenchmarkResults : public content::RenderingBenchmarkResults {
 public:
  explicit V8BenchmarkResults()
    : results_array_(v8::Array::New(0)) { }
  virtual ~V8BenchmarkResults() {}

  void AddResult(const std::string& benchmark_name,
                 const std::string& result_name,
                 const std::string& result_unit,
                 double result) {
    v8::Handle<v8::Object> result_object = v8::Object::New();
    result_object->Set(v8::String::New("benchmarkName", 13),
                       v8::String::New(benchmark_name.c_str(), -1));
    result_object->Set(v8::String::New("resultName", 10),
                       v8::String::New(result_name.c_str(), -1));
    result_object->Set(v8::String::New("resultUnit", 10),
                       v8::String::New(result_unit.c_str(), -1));
    result_object->Set(v8::String::New("result", 6), v8::Number::New(result));

    results_array_->Set(results_array_->Length(), result_object);
  }

  v8::Handle<v8::Array> results_array() {
    return results_array_;
  }

 private:
  v8::Handle<v8::Array> results_array_;
};

class GpuBenchmarkingWrapper : public v8::Extension {
 public:
  GpuBenchmarkingWrapper() :
      v8::Extension(kGpuBenchmarkingExtensionName,
          "if (typeof(chrome) == 'undefined') {"
          "  chrome = {};"
          "};"
          "if (typeof(chrome.gpuBenchmarking) == 'undefined') {"
          "  chrome.gpuBenchmarking = {};"
          "};"
          "chrome.gpuBenchmarking.renderingStats = function() {"
          "  native function GetRenderingStats();"
          "  return GetRenderingStats();"
          "};"
          "chrome.gpuBenchmarking.printToSkPicture = function(dirname) {"
          "  native function PrintToSkPicture();"
          "  return PrintToSkPicture(dirname);"
          "};"
          "chrome.gpuBenchmarking.beginSmoothScrollDown = "
          "    function(scroll_far) {"
          "  scroll_far = scroll_far || false;"
          "  native function BeginSmoothScroll();"
          "  return BeginSmoothScroll(true, scroll_far);"
          "};"
          "chrome.gpuBenchmarking.beginSmoothScrollUp = function(scroll_far) {"
          "  scroll_far = scroll_far || false;"
          "  native function BeginSmoothScroll();"
          "  return BeginSmoothScroll(false, scroll_far);"
          "};"
          "chrome.gpuBenchmarking.runRenderingBenchmarks = function(filter) {"
          "  native function RunRenderingBenchmarks();"
          "  return RunRenderingBenchmarks(filter);"
          "};") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("GetRenderingStats")))
      return v8::FunctionTemplate::New(GetRenderingStats);
    if (name->Equals(v8::String::New("PrintToSkPicture")))
      return v8::FunctionTemplate::New(PrintToSkPicture);
    if (name->Equals(v8::String::New("BeginSmoothScroll")))
      return v8::FunctionTemplate::New(BeginSmoothScroll);
    if (name->Equals(v8::String::New("RunRenderingBenchmarks")))
      return v8::FunctionTemplate::New(RunRenderingBenchmarks);

    return v8::Handle<v8::FunctionTemplate>();
  }

  static v8::Handle<v8::Value> GetRenderingStats(const v8::Arguments& args) {
    WebFrame* web_frame = WebFrame::frameForEnteredContext();
    if (!web_frame)
      return v8::Undefined();

    WebView* web_view = web_frame->view();
    if (!web_view)
      return v8::Undefined();

    RenderViewImpl* render_view_impl = RenderViewImpl::FromWebView(web_view);
    if (!render_view_impl)
      return v8::Undefined();

    WebRenderingStats stats;
    render_view_impl->GetRenderingStats(stats);

    v8::Handle<v8::Object> stats_object = v8::Object::New();
    stats_object->Set(v8::String::New("numAnimationFrames"),
                      v8::Integer::New(stats.numAnimationFrames));
    stats_object->Set(v8::String::New("numFramesSentToScreen"),
                      v8::Integer::New(stats.numFramesSentToScreen));
    stats_object->Set(v8::String::New("droppedFrameCount"),
                      v8::Integer::New(stats.droppedFrameCount));
    stats_object->Set(v8::String::New("totalPaintTimeInSeconds"),
                      v8::Number::New(stats.totalPaintTimeInSeconds));
    stats_object->Set(v8::String::New("totalRasterizeTimeInSeconds"),
                      v8::Number::New(stats.totalRasterizeTimeInSeconds));
    return stats_object;
  }

  static v8::Handle<v8::Value> PrintToSkPicture(const v8::Arguments& args) {
    if (args.Length() != 1)
      return v8::Undefined();

    v8::String::AsciiValue dirname(args[0]);
    if (dirname.length() == 0)
      return v8::Undefined();

    WebFrame* web_frame = WebFrame::frameForEnteredContext();
    if (!web_frame)
      return v8::Undefined();

    WebView* web_view = web_frame->view();
    if (!web_view)
      return v8::Undefined();

    WebViewBenchmarkSupport* benchmark_support = web_view->benchmarkSupport();
    if (!benchmark_support)
      return v8::Undefined();

    FilePath dirpath;
    dirpath = dirpath.AppendASCII(*dirname);
    if (!file_util::CreateDirectory(dirpath) ||
        !file_util::PathIsWritable(dirpath)) {
      std::string msg("Path is not writable: ");
      msg.append(dirpath.MaybeAsASCII());
      return v8::ThrowException(v8::Exception::Error(
          v8::String::New(msg.c_str(), msg.length())));
    }

    SkPictureRecorder recorder(dirpath);
    benchmark_support->paint(&recorder,
                             WebViewBenchmarkSupport::PaintModeEverything);
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> BeginSmoothScroll(const v8::Arguments& args) {
    WebFrame* web_frame = WebFrame::frameForEnteredContext();
    if (!web_frame)
      return v8::Undefined();

    WebView* web_view = web_frame->view();
    if (!web_view)
      return v8::Undefined();

    RenderViewImpl* render_view_impl = RenderViewImpl::FromWebView(web_view);
    if (!render_view_impl)
      return v8::Undefined();

    if (args.Length() != 2 || !args[0]->IsBoolean() || !args[1]->IsBoolean())
      return v8::False();

    bool scroll_down = args[0]->BooleanValue();
    bool scroll_far = args[1]->BooleanValue();

    render_view_impl->BeginSmoothScroll(scroll_down, scroll_far);
    return v8::True();
  }

  static v8::Handle<v8::Value> RunRenderingBenchmarks(
      const v8::Arguments& args) {
    // For our name filter, the argument can be undefined or null to run
    // all benchmarks, or a string for filtering by name.
    if (!args.Length() ||
        (!args[0]->IsString() &&
         !(args[0]->IsNull() || args[0]->IsUndefined()))) {
      return v8::Undefined();
    }

    std::string name_filter;
    if (args[0]->IsNull() || args[0]->IsUndefined()) {
      name_filter = "";
    } else {
      char filter[256];
      args[0]->ToString()->WriteAscii(filter, 0, sizeof(filter)-1);
      name_filter = std::string(filter);
    }

    WebFrame* web_frame = WebFrame::frameForEnteredContext();
    if (!web_frame)
      return v8::Undefined();

    WebView* web_view = web_frame->view();
    if (!web_view)
      return v8::Undefined();

    WebViewBenchmarkSupport* support = web_view->benchmarkSupport();
    if (!support)
      return v8::Undefined();

    ScopedVector<RenderingBenchmark> benchmarks = AllRenderingBenchmarks();

    V8BenchmarkResults results;
    ScopedVector<RenderingBenchmark>::const_iterator it;
    for (it = benchmarks.begin(); it != benchmarks.end(); it++) {
      RenderingBenchmark* benchmark = *it;
      const std::string& name = benchmark->name();
      if (name_filter != "" &&
          std::string::npos == name.find(name_filter)) {
        continue;
      }
      benchmark->SetUp(support);
      benchmark->Run(&results, support);
      benchmark->TearDown(support);
    }

    return results.results_array();
  }
};

v8::Extension* GpuBenchmarkingExtension::Get() {
  return new GpuBenchmarkingWrapper();
}

}  // namespace content
