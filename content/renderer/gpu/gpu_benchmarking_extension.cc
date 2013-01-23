// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_benchmarking_extension.h"

#include <string>

#include "base/base64.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_vector.h"
#include "base/string_number_conversions.h"
#include "content/common/gpu/gpu_rendering_stats.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/all_rendering_benchmarks.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/rendering_benchmark.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewBenchmarkSupport.h"
#include "v8/include/v8.h"
#include "webkit/compositor_bindings/web_rendering_stats_impl.h"

using WebKit::WebCanvas;
using WebKit::WebFrame;
using WebKit::WebPrivatePtr;
using WebKit::WebRenderingStatsImpl;
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

class RenderingStatsEnumerator : public cc::RenderingStats::Enumerator {
 public:
  RenderingStatsEnumerator(v8::Handle<v8::Object> stats_object)
      : stats_object(stats_object) { }

  virtual void AddInt64(const char* name, int64 value) {
    stats_object->Set(v8::String::New(name), v8::Number::New(value));
  }

  virtual void AddDouble(const char* name, double value) {
    stats_object->Set(v8::String::New(name), v8::Number::New(value));
  }

  virtual void AddInt(const char* name, int value) {
    stats_object->Set(v8::String::New(name), v8::Integer::New(value));
  }

  virtual void AddTimeDeltaInSecondsF(const char* name,
                                      const base::TimeDelta& value) {
    stats_object->Set(v8::String::New(name),
                      v8::Number::New(value.InSecondsF()));
  }

 private:
  v8::Handle<v8::Object> stats_object;
};

}  // namespace

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
          "};"
          "chrome.gpuBenchmarking.printToSkPicture = function(dirname) {"
          "  native function PrintToSkPicture();"
          "  return PrintToSkPicture(dirname);"
          "};"
          "chrome.gpuBenchmarking.smoothScrollBy = "
          "    function(pixels_to_scroll, opt_callback, opt_mouse_event_x,"
          "             opt_mouse_event_y) {"
          "  pixels_to_scroll = pixels_to_scroll || 0;"
          "  callback = opt_callback || function() { };"
          "  native function BeginSmoothScroll();"
          "  if (typeof opt_mouse_event_x !== 'undefined' &&"
          "      typeof opt_mouse_event_y !== 'undefined') {"
          "    return BeginSmoothScroll(pixels_to_scroll >= 0, callback,"
          "                             Math.abs(pixels_to_scroll),"
          "                             opt_mouse_event_x, opt_mouse_event_y);"
          "  } else {"
          "    return BeginSmoothScroll(pixels_to_scroll >= 0, callback,"
          "                             Math.abs(pixels_to_scroll));"
          "  }"
          "};"
          "chrome.gpuBenchmarking.runRenderingBenchmarks = function(filter) {"
          "  native function RunRenderingBenchmarks();"
          "  return RunRenderingBenchmarks(filter);"
          "};"
          "chrome.gpuBenchmarking.beginWindowSnapshotPNG = function(callback) {"
          "  native function BeginWindowSnapshotPNG();"
          "  BeginWindowSnapshotPNG(callback);"
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
    if (name->Equals(v8::String::New("BeginWindowSnapshotPNG")))
      return v8::FunctionTemplate::New(BeginWindowSnapshotPNG);

    return v8::Handle<v8::FunctionTemplate>();
  }

  static v8::Handle<v8::Value> GetRenderingStats(const v8::Arguments& args) {

    WebFrame* web_frame = WebFrame::frameForCurrentContext();
    if (!web_frame)
      return v8::Undefined();

    WebView* web_view = web_frame->view();
    if (!web_view)
      return v8::Undefined();

    RenderViewImpl* render_view_impl = RenderViewImpl::FromWebView(web_view);
    if (!render_view_impl)
      return v8::Undefined();

    WebRenderingStatsImpl stats;
    render_view_impl->GetRenderingStats(stats);

    content::GpuRenderingStats gpu_stats;
    render_view_impl->GetGpuRenderingStats(&gpu_stats);
    v8::Handle<v8::Object> stats_object = v8::Object::New();

    RenderingStatsEnumerator enumerator(stats_object);
    stats.rendering_stats.EnumerateFields(&enumerator);
    gpu_stats.EnumerateFields(&enumerator);

    return stats_object;
  }

  static v8::Handle<v8::Value> PrintToSkPicture(const v8::Arguments& args) {
    if (args.Length() != 1)
      return v8::Undefined();

    v8::String::AsciiValue dirname(args[0]);
    if (dirname.length() == 0)
      return v8::Undefined();

    WebFrame* web_frame = WebFrame::frameForCurrentContext();
    if (!web_frame)
      return v8::Undefined();

    WebView* web_view = web_frame->view();
    if (!web_view)
      return v8::Undefined();

    WebViewBenchmarkSupport* benchmark_support = web_view->benchmarkSupport();
    if (!benchmark_support)
      return v8::Undefined();

    FilePath dirpath(FilePath::StringType(*dirname,
                                          *dirname + dirname.length()));
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

  static void OnSmoothScrollCompleted(v8::Persistent<v8::Function> callback,
                                      v8::Persistent<v8::Context> context) {
    v8::HandleScope scope;
    v8::Context::Scope context_scope(context);
    WebFrame* frame = WebFrame::frameForContext(context);
    if (frame) {
      frame->callFunctionEvenIfScriptDisabled(callback,
                                              v8::Object::New(),
                                              0,
                                              NULL);
    }
    callback.Dispose();
    context.Dispose();
  }

  static v8::Handle<v8::Value> BeginSmoothScroll(const v8::Arguments& args) {
    WebFrame* web_frame = WebFrame::frameForCurrentContext();
    if (!web_frame)
      return v8::Undefined();

    WebView* web_view = web_frame->view();
    if (!web_view)
      return v8::Undefined();

    RenderViewImpl* render_view_impl = RenderViewImpl::FromWebView(web_view);
    if (!render_view_impl)
      return v8::Undefined();

    // Account for the 2 optional arguments, mouse_event_x and mouse_event_y.
    int arglen = args.Length();
    if (arglen < 3 ||
        !args[0]->IsBoolean() ||
        !args[1]->IsFunction() ||
        !args[2]->IsNumber())
      return v8::False();

    bool scroll_down = args[0]->BooleanValue();
    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>(v8::Function::Cast(*args[1]));
    v8::Persistent<v8::Function> callback =
        v8::Persistent<v8::Function>::New(callback_local);
    v8::Persistent<v8::Context> context =
        v8::Persistent<v8::Context>::New(web_frame->mainWorldScriptContext());

    int pixels_to_scroll = args[2]->IntegerValue();

    int mouse_event_x = 0;
    int mouse_event_y = 0;

    if (arglen == 3) {
      WebKit::WebRect rect = render_view_impl->windowRect();
      mouse_event_x = rect.x + rect.width / 2;
      mouse_event_y = rect.y + rect.height / 2;
    } else {
      if (arglen != 5 ||
          !args[3]->IsNumber() ||
          !args[4]->IsNumber())
        return v8::False();

      mouse_event_x = args[3]->IntegerValue();
      mouse_event_y = args[4]->IntegerValue();
    }

    // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
    // progress, we will leak the callback and context. This needs to be fixed,
    // somehow.
    render_view_impl->BeginSmoothScroll(
        scroll_down,
        base::Bind(&OnSmoothScrollCompleted,
                   callback,
                   context),
        pixels_to_scroll,
        mouse_event_x,
        mouse_event_y);

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

    WebFrame* web_frame = WebFrame::frameForCurrentContext();
    if (!web_frame)
      return v8::Undefined();

    WebView* web_view = web_frame->view();
    if (!web_view)
      return v8::Undefined();

    WebViewBenchmarkSupport* support = web_view->benchmarkSupport();
    if (!support)
      return v8::Undefined();

    ScopedVector<RenderingBenchmark> benchmarks = AllRenderingBenchmarks();

    v8::Handle<v8::Array> results = v8::Array::New(0);
    ScopedVector<RenderingBenchmark>::const_iterator it;
    for (it = benchmarks.begin(); it != benchmarks.end(); it++) {
      RenderingBenchmark* benchmark = *it;
      const std::string& name = benchmark->name();
      if (name_filter != "" &&
          std::string::npos == name.find(name_filter)) {
        continue;
      }
      benchmark->SetUp(support);
      double result = benchmark->Run(support);
      benchmark->TearDown(support);

      v8::Handle<v8::Object> result_object = v8::Object::New();
      result_object->Set(v8::String::New("benchmark", 9),
                         v8::String::New(name.c_str(), -1));
      result_object->Set(v8::String::New("result", 6), v8::Number::New(result));
      results->Set(results->Length(), result_object);
    }

    return results;
  }

  static void OnSnapshotCompleted(v8::Persistent<v8::Function> callback,
                                  v8::Persistent<v8::Context> context,
                                  const gfx::Size& size,
                                  const std::vector<unsigned char>& png) {
    v8::HandleScope scope;
    v8::Context::Scope context_scope(context);
    WebFrame* frame = WebFrame::frameForContext(context);
    if (frame) {

      v8::Handle<v8::Value> result;

      if(!size.IsEmpty()) {
        v8::Handle<v8::Object> result_object;
        result_object = v8::Object::New();

        result_object->Set(v8::String::New("width"),
                           v8::Number::New(size.width()));
        result_object->Set(v8::String::New("height"),
                           v8::Number::New(size.height()));

        std::string base64_png;
        base::Base64Encode(base::StringPiece(
            reinterpret_cast<const char*>(&*png.begin()), png.size()),
            &base64_png);

        result_object->Set(v8::String::New("data"),
            v8::String::New(base64_png.c_str(), base64_png.size()));

        result = result_object;
      } else {
        result = v8::Null();
      }

      v8::Handle<v8::Value> argv[] = { result };

      frame->callFunctionEvenIfScriptDisabled(callback,
                                              v8::Object::New(),
                                              1,
                                              argv);
    }
    callback.Dispose();
    context.Dispose();
  }

  static v8::Handle<v8::Value> BeginWindowSnapshotPNG(
      const v8::Arguments& args) {
    WebFrame* web_frame = WebFrame::frameForCurrentContext();
    if (!web_frame)
      return v8::Undefined();

    WebView* web_view = web_frame->view();
    if (!web_view)
      return v8::Undefined();

    RenderViewImpl* render_view_impl = RenderViewImpl::FromWebView(web_view);
    if (!render_view_impl)
      return v8::Undefined();

    if (!args[0]->IsFunction())
      return v8::Undefined();

    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>(v8::Function::Cast(*args[0]));
    v8::Persistent<v8::Function> callback =
        v8::Persistent<v8::Function>::New(callback_local);
    v8::Persistent<v8::Context> context =
        v8::Persistent<v8::Context>::New(web_frame->mainWorldScriptContext());

    render_view_impl->GetWindowSnapshot(
        base::Bind(&OnSnapshotCompleted, callback, context));

    return v8::Undefined();
  }
};

v8::Extension* GpuBenchmarkingExtension::Get() {
  return new GpuBenchmarkingWrapper();
}

}  // namespace content
