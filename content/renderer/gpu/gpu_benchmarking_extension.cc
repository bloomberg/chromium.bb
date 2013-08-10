// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_benchmarking_extension.h"

#include <string>

#include "base/base64.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/browser_rendering_stats.h"
#include "content/common/gpu/gpu_rendering_stats.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/rendering_benchmark.h"
#include "content/renderer/skia_benchmarking_extension.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebImageCache.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebViewBenchmarkSupport.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/codec/png_codec.h"
#include "v8/include/v8.h"
#include "webkit/renderer/compositor_bindings/web_rendering_stats_impl.h"

using WebKit::WebCanvas;
using WebKit::WebFrame;
using WebKit::WebImageCache;
using WebKit::WebPrivatePtr;
using WebKit::WebRenderingStatsImpl;
using WebKit::WebSize;
using WebKit::WebView;
using WebKit::WebViewBenchmarkSupport;

const char kGpuBenchmarkingExtensionName[] = "v8/GpuBenchmarking";

static SkData* EncodeBitmapToData(size_t* offset, const SkBitmap& bm) {
    SkPixelRef* pr = bm.pixelRef();
    if (pr != NULL) {
        SkData* data = pr->refEncodedData();
        if (data != NULL) {
            *offset = bm.pixelRefOffset();
            return data;
        }
    }
    std::vector<unsigned char> vector;
    if (gfx::PNGCodec::EncodeBGRASkBitmap(bm, false, &vector)) {
        return SkData::NewWithCopy(&vector.front() , vector.size());
    }
    return NULL;
}

namespace {

class SkPictureRecorder : public WebViewBenchmarkSupport::PaintClient {
 public:
  explicit SkPictureRecorder(const base::FilePath& dirpath)
      : dirpath_(dirpath),
        layer_id_(0) {
    // Let skia register known effect subclasses. This basically enables
    // reflection on those subclasses required for picture serialization.
    content::SkiaBenchmarkingExtension::InitSkGraphics();
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
    picture_.serialize(&file, &EncodeBitmapToData);
  }

 private:
  base::FilePath dirpath_;
  int layer_id_;
  SkPicture picture_;
};

class RenderingStatsEnumerator : public cc::RenderingStats::Enumerator {
 public:
  RenderingStatsEnumerator(v8::Handle<v8::Object> stats_object)
      : stats_object(stats_object) { }

  virtual void AddInt64(const char* name, int64 value) OVERRIDE {
    stats_object->Set(v8::String::New(name), v8::Number::New(value));
  }

  virtual void AddDouble(const char* name, double value) OVERRIDE {
    stats_object->Set(v8::String::New(name), v8::Number::New(value));
  }

  virtual void AddInt(const char* name, int value) OVERRIDE {
    stats_object->Set(v8::String::New(name), v8::Integer::New(value));
  }

  virtual void AddTimeDeltaInSecondsF(const char* name,
                                      const base::TimeDelta& value) OVERRIDE {
    stats_object->Set(v8::String::New(name),
                      v8::Number::New(value.InSecondsF()));
  }

 private:
  v8::Handle<v8::Object> stats_object;
};

}  // namespace

namespace content {

namespace {

class CallbackAndContext : public base::RefCounted<CallbackAndContext> {
 public:
  CallbackAndContext(v8::Isolate* isolate,
                     v8::Handle<v8::Function> callback,
                     v8::Handle<v8::Context> context)
      : isolate_(isolate) {
    callback_.Reset(isolate_, callback);
    context_.Reset(isolate_, context);
  }

  v8::Isolate* isolate() {
    return isolate_;
  }

  v8::Handle<v8::Function> GetCallback() {
    return v8::Local<v8::Function>::New(isolate_, callback_);
  }

  v8::Handle<v8::Context> GetContext() {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

 private:
  friend class base::RefCounted<CallbackAndContext>;

  virtual ~CallbackAndContext() {
    callback_.Dispose();
    context_.Dispose();
  }

  v8::Isolate* isolate_;
  v8::Persistent<v8::Function> callback_;
  v8::Persistent<v8::Context> context_;
  DISALLOW_COPY_AND_ASSIGN(CallbackAndContext);
};

}  // namespace

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
          "chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers = function() {"
          "  native function SetNeedsDisplayOnAllLayers();"
          "  return SetNeedsDisplayOnAllLayers();"
          "};"
          "chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent = function() {"
          "  native function SetRasterizeOnlyVisibleContent();"
          "  return SetRasterizeOnlyVisibleContent();"
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
          "chrome.gpuBenchmarking.smoothScrollBySendsTouch = function() {"
          "  native function SmoothScrollSendsTouch();"
          "  return SmoothScrollSendsTouch();"
          "};"
          "chrome.gpuBenchmarking.beginWindowSnapshotPNG = function(callback) {"
          "  native function BeginWindowSnapshotPNG();"
          "  BeginWindowSnapshotPNG(callback);"
          "};"
          "chrome.gpuBenchmarking.clearImageCache = function() {"
          "  native function ClearImageCache();"
          "  ClearImageCache();"
          "};") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(v8::String::New("SetNeedsDisplayOnAllLayers")))
      return v8::FunctionTemplate::New(SetNeedsDisplayOnAllLayers);
    if (name->Equals(v8::String::New("SetRasterizeOnlyVisibleContent")))
      return v8::FunctionTemplate::New(SetRasterizeOnlyVisibleContent);
    if (name->Equals(v8::String::New("GetRenderingStats")))
      return v8::FunctionTemplate::New(GetRenderingStats);
    if (name->Equals(v8::String::New("PrintToSkPicture")))
      return v8::FunctionTemplate::New(PrintToSkPicture);
    if (name->Equals(v8::String::New("BeginSmoothScroll")))
      return v8::FunctionTemplate::New(BeginSmoothScroll);
    if (name->Equals(v8::String::New("SmoothScrollSendsTouch")))
      return v8::FunctionTemplate::New(SmoothScrollSendsTouch);
    if (name->Equals(v8::String::New("BeginWindowSnapshotPNG")))
      return v8::FunctionTemplate::New(BeginWindowSnapshotPNG);
    if (name->Equals(v8::String::New("ClearImageCache")))
      return v8::FunctionTemplate::New(ClearImageCache);

    return v8::Handle<v8::FunctionTemplate>();
  }

  static void SetNeedsDisplayOnAllLayers(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    WebFrame* web_frame = WebFrame::frameForCurrentContext();
    if (!web_frame)
      return;

    WebView* web_view = web_frame->view();
    if (!web_view)
      return;

    RenderViewImpl* render_view_impl = RenderViewImpl::FromWebView(web_view);
    if (!render_view_impl)
      return;

    RenderWidgetCompositor* compositor = render_view_impl->compositor();
    if (!compositor)
      return;

    compositor->SetNeedsDisplayOnAllLayers();
  }

  static void SetRasterizeOnlyVisibleContent(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    WebFrame* web_frame = WebFrame::frameForCurrentContext();
    if (!web_frame)
      return;

    WebView* web_view = web_frame->view();
    if (!web_view)
      return;

    RenderViewImpl* render_view_impl = RenderViewImpl::FromWebView(web_view);
    if (!render_view_impl)
      return;

    RenderWidgetCompositor* compositor = render_view_impl->compositor();
    if (!compositor)
      return;

    compositor->SetRasterizeOnlyVisibleContent();
  }

  static void GetRenderingStats(
      const v8::FunctionCallbackInfo<v8::Value>& args) {

    WebFrame* web_frame = WebFrame::frameForCurrentContext();
    if (!web_frame)
      return;

    WebView* web_view = web_frame->view();
    if (!web_view)
      return;

    RenderViewImpl* render_view_impl = RenderViewImpl::FromWebView(web_view);
    if (!render_view_impl)
      return;

    WebRenderingStatsImpl stats;
    render_view_impl->GetRenderingStats(stats);

    content::GpuRenderingStats gpu_stats;
    render_view_impl->GetGpuRenderingStats(&gpu_stats);
    BrowserRenderingStats browser_stats;
    render_view_impl->GetBrowserRenderingStats(&browser_stats);
    v8::Handle<v8::Object> stats_object = v8::Object::New();

    RenderingStatsEnumerator enumerator(stats_object);
    stats.rendering_stats.EnumerateFields(&enumerator);
    gpu_stats.EnumerateFields(&enumerator);
    browser_stats.EnumerateFields(&enumerator);

    args.GetReturnValue().Set(stats_object);
  }

  static void PrintToSkPicture(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1)
      return;

    v8::String::AsciiValue dirname(args[0]);
    if (dirname.length() == 0)
      return;

    WebFrame* web_frame = WebFrame::frameForCurrentContext();
    if (!web_frame)
      return;

    WebView* web_view = web_frame->view();
    if (!web_view)
      return;

    WebViewBenchmarkSupport* benchmark_support = web_view->benchmarkSupport();
    if (!benchmark_support)
      return;

    base::FilePath dirpath(
        base::FilePath::StringType(*dirname, *dirname + dirname.length()));
    if (!file_util::CreateDirectory(dirpath) ||
        !base::PathIsWritable(dirpath)) {
      std::string msg("Path is not writable: ");
      msg.append(dirpath.MaybeAsASCII());
      v8::ThrowException(v8::Exception::Error(
          v8::String::New(msg.c_str(), msg.length())));
      return;
    }

    SkPictureRecorder recorder(dirpath);
    benchmark_support->paint(&recorder,
                             WebViewBenchmarkSupport::PaintModeEverything);
  }

  static void OnSmoothScrollCompleted(
      CallbackAndContext* callback_and_context) {
    v8::HandleScope scope(callback_and_context->isolate());
    v8::Handle<v8::Context> context = callback_and_context->GetContext();
    v8::Context::Scope context_scope(context);
    WebFrame* frame = WebFrame::frameForContext(context);
    if (frame) {
      frame->callFunctionEvenIfScriptDisabled(
          callback_and_context->GetCallback(), v8::Object::New(), 0, NULL);
    }
  }

  static void SmoothScrollSendsTouch(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    // TODO(epenner): Should other platforms emulate touch events?
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    args.GetReturnValue().Set(true);
#else
    args.GetReturnValue().Set(false);
#endif
  }

  static void BeginSmoothScroll(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    WebFrame* web_frame = WebFrame::frameForCurrentContext();
    if (!web_frame)
      return;

    WebView* web_view = web_frame->view();
    if (!web_view)
      return;

    RenderViewImpl* render_view_impl = RenderViewImpl::FromWebView(web_view);
    if (!render_view_impl)
      return;

    // Account for the 2 optional arguments, mouse_event_x and mouse_event_y.
    int arglen = args.Length();
    if (arglen < 3 ||
        !args[0]->IsBoolean() ||
        !args[1]->IsFunction() ||
        !args[2]->IsNumber()) {
      args.GetReturnValue().Set(false);
      return;
    }

    bool scroll_down = args[0]->BooleanValue();
    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>::Cast(args[1]);

    scoped_refptr<CallbackAndContext> callback_and_context =
        new CallbackAndContext(args.GetIsolate(),
                               callback_local,
                               web_frame->mainWorldScriptContext());

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
          !args[4]->IsNumber()) {
        args.GetReturnValue().Set(false);
        return;
      }

      mouse_event_x = args[3]->IntegerValue() * web_view->pageScaleFactor();
      mouse_event_y = args[4]->IntegerValue() * web_view->pageScaleFactor();
    }

    // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
    // progress, we will leak the callback and context. This needs to be fixed,
    // somehow.
    render_view_impl->BeginSmoothScroll(
        scroll_down,
        base::Bind(&OnSmoothScrollCompleted,
                   callback_and_context),
        pixels_to_scroll,
        mouse_event_x,
        mouse_event_y);

    args.GetReturnValue().Set(true);
  }

  static void OnSnapshotCompleted(CallbackAndContext* callback_and_context,
                                  const gfx::Size& size,
                                  const std::vector<unsigned char>& png) {
    v8::HandleScope scope(callback_and_context->isolate());
    v8::Handle<v8::Context> context = callback_and_context->GetContext();
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

      frame->callFunctionEvenIfScriptDisabled(
          callback_and_context->GetCallback(), v8::Object::New(), 1, argv);
    }
  }

  static void BeginWindowSnapshotPNG(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    WebFrame* web_frame = WebFrame::frameForCurrentContext();
    if (!web_frame)
      return;

    WebView* web_view = web_frame->view();
    if (!web_view)
      return;

    RenderViewImpl* render_view_impl = RenderViewImpl::FromWebView(web_view);
    if (!render_view_impl)
      return;

    if (!args[0]->IsFunction())
      return;

    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>::Cast(args[0]);

    scoped_refptr<CallbackAndContext> callback_and_context =
        new CallbackAndContext(args.GetIsolate(),
                               callback_local,
                               web_frame->mainWorldScriptContext());

    render_view_impl->GetWindowSnapshot(
        base::Bind(&OnSnapshotCompleted, callback_and_context));
  }

  static void ClearImageCache(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    WebImageCache::clear();
  }
};

v8::Extension* GpuBenchmarkingExtension::Get() {
  return new GpuBenchmarkingWrapper();
}

}  // namespace content
