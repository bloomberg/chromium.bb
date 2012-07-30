// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_benchmarking_extension.h"

#include <string>

#include "base/memory/scoped_vector.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/all_rendering_benchmarks.h"
#include "content/renderer/rendering_benchmark.h"
#include "content/renderer/rendering_benchmark_results.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRenderingStats.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewBenchmarkSupport.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;
using WebKit::WebPrivatePtr;
using WebKit::WebViewBenchmarkSupport;
using WebKit::WebRenderingStats;
using WebKit::WebView;

const char kGpuBenchmarkingExtensionName[] = "v8/GpuBenchmarking";

using WebKit::WebFrame;
using WebKit::WebView;

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

    WebRenderingStats stats;
    web_view->renderingStats(stats);

    v8::Handle<v8::Object> stats_object = v8::Object::New();
    if (stats.numAnimationFrames)
      stats_object->Set(v8::String::New("numAnimationFrames"),
                        v8::Integer::New(stats.numAnimationFrames),
                        v8::ReadOnly);
    if (stats.numFramesSentToScreen)
      stats_object->Set(v8::String::New("numFramesSentToScreen"),
                        v8::Integer::New(stats.numFramesSentToScreen),
                        v8::ReadOnly);
    if (stats.droppedFrameCount)
      stats_object->Set(v8::String::New("droppedFrameCount"),
                        v8::Integer::New(stats.droppedFrameCount),
                        v8::ReadOnly);
    if (stats.totalPaintTimeInSeconds)
      stats_object->Set(v8::String::New("totalPaintTimeInSeconds"),
                        v8::Number::New(stats.totalPaintTimeInSeconds),
                        v8::ReadOnly);
    if (stats.totalRasterizeTimeInSeconds)
      stats_object->Set(v8::String::New("totalRasterizeTimeInSeconds"),
                        v8::Number::New(stats.totalRasterizeTimeInSeconds),
                        v8::ReadOnly);
    return stats_object;
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
