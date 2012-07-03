// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_benchmarking_extension.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebRenderingStats.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;
using WebKit::WebRenderingStats;
using WebKit::WebView;

const char kGpuBenchmarkingExtensionName[] = "v8/GpuBenchmarking";

namespace content {

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
          "};") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) {
    if (name->Equals(v8::String::New("GetRenderingStats")))
      return v8::FunctionTemplate::New(GetRenderingStats);

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
    return stats_object;
  }
};

v8::Extension* GpuBenchmarkingExtension::Get() {
  return new GpuBenchmarkingWrapper();
}

}  // namespace content
