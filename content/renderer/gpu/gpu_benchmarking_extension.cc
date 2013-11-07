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
#include "cc/layers/layer.h"
#include "content/common/browser_rendering_stats.h"
#include "content/common/gpu/gpu_rendering_stats.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/skia_benchmarking_extension.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebImageCache.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/codec/png_codec.h"
#include "v8/include/v8.h"
#include "webkit/renderer/compositor_bindings/web_rendering_stats_impl.h"

using blink::WebCanvas;
using blink::WebFrame;
using blink::WebImageCache;
using blink::WebPrivatePtr;
using blink::WebRenderingStatsImpl;
using blink::WebSize;
using blink::WebView;

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

class SkPictureSerializer {
 public:
  explicit SkPictureSerializer(const base::FilePath& dirpath)
      : dirpath_(dirpath),
        layer_id_(0) {
    // Let skia register known effect subclasses. This basically enables
    // reflection on those subclasses required for picture serialization.
    content::SkiaBenchmarkingExtension::InitSkGraphics();
  }

  // Recursively serializes the layer tree.
  // Each layer in the tree is serialized into a separate skp file
  // in the given directory.
  void Serialize(const cc::Layer* layer) {
    const cc::LayerList& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i) {
      Serialize(children[i].get());
    }

    skia::RefPtr<SkPicture> picture = layer->GetPicture();
    if (!picture)
      return;

    // Serialize picture to file.
    // TODO(alokp): Note that for this to work Chrome needs to be launched with
    // --no-sandbox command-line flag. Get rid of this limitation.
    // CRBUG: 139640.
    std::string filename = "layer_" + base::IntToString(layer_id_++) + ".skp";
    std::string filepath = dirpath_.AppendASCII(filename).MaybeAsASCII();
    DCHECK(!filepath.empty());
    SkFILEWStream file(filepath.c_str());
    DCHECK(file.isValid());
    picture->serialize(&file, &EncodeBitmapToData);
  }

 private:
  base::FilePath dirpath_;
  int layer_id_;
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

class GpuBenchmarkingContext {
 public:
  GpuBenchmarkingContext()
      : web_frame_(NULL),
        web_view_(NULL),
        render_view_impl_(NULL),
        compositor_(NULL) {}

  bool Init(bool init_compositor) {
    web_frame_ = WebFrame::frameForCurrentContext();
    if (!web_frame_)
      return false;

    web_view_ = web_frame_->view();
    if (!web_view_) {
      web_frame_ = NULL;
      return false;
    }

    render_view_impl_ = RenderViewImpl::FromWebView(web_view_);
    if (!render_view_impl_) {
      web_frame_ = NULL;
      web_view_ = NULL;
      return false;
    }

    if (!init_compositor)
      return true;

    compositor_ = render_view_impl_->compositor();
    if (!compositor_) {
      web_frame_ = NULL;
      web_view_ = NULL;
      render_view_impl_ = NULL;
      return false;
    }

    return true;
  }

  WebFrame* web_frame() const {
    DCHECK(web_frame_ != NULL);
    return web_frame_;
  }
  WebView* web_view() const {
    DCHECK(web_view_ != NULL);
    return web_view_;
  }
  RenderViewImpl* render_view_impl() const {
    DCHECK(render_view_impl_ != NULL);
    return render_view_impl_;
  }
  RenderWidgetCompositor* compositor() const {
    DCHECK(compositor_ != NULL);
    return compositor_;
  }

 private:
  WebFrame* web_frame_;
  WebView* web_view_;
  RenderViewImpl* render_view_impl_;
  RenderWidgetCompositor* compositor_;

  DISALLOW_COPY_AND_ASSIGN(GpuBenchmarkingContext);
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
          "chrome.gpuBenchmarking.gpuRenderingStats = function() {"
          "  native function GetGpuRenderingStats();"
          "  return GetGpuRenderingStats();"
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
          "chrome.gpuBenchmarking.pinchBy = "
          "    function(zoom_in, pixels_to_move, anchor_x, anchor_y,"
          "             opt_callback) {"
          "  callback = opt_callback || function() { };"
          "  native function BeginPinch();"
          "  return BeginPinch(zoom_in, pixels_to_move,"
          "                    anchor_x, anchor_y, callback);"
          "};"
          "chrome.gpuBenchmarking.beginWindowSnapshotPNG = function(callback) {"
          "  native function BeginWindowSnapshotPNG();"
          "  BeginWindowSnapshotPNG(callback);"
          "};"
          "chrome.gpuBenchmarking.clearImageCache = function() {"
          "  native function ClearImageCache();"
          "  ClearImageCache();"
          "};"
          "chrome.gpuBenchmarking.runMicroBenchmark ="
          "    function(name, callback, opt_arguments) {"
          "  arguments = opt_arguments || {};"
          "  native function RunMicroBenchmark();"
          "  return RunMicroBenchmark(name, callback, arguments);"
          "};"
          "chrome.gpuBenchmarking.hasGpuProcess = function() {"
          "  native function HasGpuProcess();"
          "  return HasGpuProcess();"
          "};") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(v8::String::New("SetNeedsDisplayOnAllLayers")))
      return v8::FunctionTemplate::New(SetNeedsDisplayOnAllLayers);
    if (name->Equals(v8::String::New("SetRasterizeOnlyVisibleContent")))
      return v8::FunctionTemplate::New(SetRasterizeOnlyVisibleContent);
    if (name->Equals(v8::String::New("GetRenderingStats")))
      return v8::FunctionTemplate::New(GetRenderingStats);
    if (name->Equals(v8::String::New("GetGpuRenderingStats")))
      return v8::FunctionTemplate::New(GetGpuRenderingStats);
    if (name->Equals(v8::String::New("PrintToSkPicture")))
      return v8::FunctionTemplate::New(PrintToSkPicture);
    if (name->Equals(v8::String::New("BeginSmoothScroll")))
      return v8::FunctionTemplate::New(BeginSmoothScroll);
    if (name->Equals(v8::String::New("SmoothScrollSendsTouch")))
      return v8::FunctionTemplate::New(SmoothScrollSendsTouch);
    if (name->Equals(v8::String::New("BeginPinch")))
      return v8::FunctionTemplate::New(BeginPinch);
    if (name->Equals(v8::String::New("BeginWindowSnapshotPNG")))
      return v8::FunctionTemplate::New(BeginWindowSnapshotPNG);
    if (name->Equals(v8::String::New("ClearImageCache")))
      return v8::FunctionTemplate::New(ClearImageCache);
    if (name->Equals(v8::String::New("RunMicroBenchmark")))
      return v8::FunctionTemplate::New(RunMicroBenchmark);
    if (name->Equals(v8::String::New("HasGpuProcess")))
      return v8::FunctionTemplate::New(HasGpuProcess);

    return v8::Handle<v8::FunctionTemplate>();
  }

  static void SetNeedsDisplayOnAllLayers(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuBenchmarkingContext context;
    if (!context.Init(true))
      return;

    context.compositor()->SetNeedsDisplayOnAllLayers();
  }

  static void SetRasterizeOnlyVisibleContent(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuBenchmarkingContext context;
    if (!context.Init(true))
      return;

    context.compositor()->SetRasterizeOnlyVisibleContent();
  }

  static void GetRenderingStats(
      const v8::FunctionCallbackInfo<v8::Value>& args) {

    GpuBenchmarkingContext context;
    if (!context.Init(false))
      return;

    WebRenderingStatsImpl stats;
    context.render_view_impl()->GetRenderingStats(stats);

    content::GpuRenderingStats gpu_stats;
    context.render_view_impl()->GetGpuRenderingStats(&gpu_stats);
    BrowserRenderingStats browser_stats;
    context.render_view_impl()->GetBrowserRenderingStats(&browser_stats);
    v8::Handle<v8::Object> stats_object = v8::Object::New();

    RenderingStatsEnumerator enumerator(stats_object);
    stats.rendering_stats.EnumerateFields(&enumerator);
    gpu_stats.EnumerateFields(&enumerator);
    browser_stats.EnumerateFields(&enumerator);

    args.GetReturnValue().Set(stats_object);
  }

  static void GetGpuRenderingStats(
      const v8::FunctionCallbackInfo<v8::Value>& args) {

    GpuBenchmarkingContext context;
    if (!context.Init(false))
      return;

    content::GpuRenderingStats gpu_stats;
    context.render_view_impl()->GetGpuRenderingStats(&gpu_stats);

    v8::Handle<v8::Object> stats_object = v8::Object::New();
    RenderingStatsEnumerator enumerator(stats_object);
    gpu_stats.EnumerateFields(&enumerator);

    args.GetReturnValue().Set(stats_object);
  }

  static void PrintToSkPicture(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1)
      return;

    v8::String::AsciiValue dirname(args[0]);
    if (dirname.length() == 0)
      return;

    GpuBenchmarkingContext context;
    if (!context.Init(true))
      return;

    const cc::Layer* root_layer = context.compositor()->GetRootLayer();
    if (!root_layer)
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

    SkPictureSerializer serializer(dirpath);
    serializer.Serialize(root_layer);
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
    GpuBenchmarkingContext context;
    if (!context.Init(false))
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
                               context.web_frame()->mainWorldScriptContext());

    int pixels_to_scroll = args[2]->IntegerValue();

    int mouse_event_x = 0;
    int mouse_event_y = 0;

    if (arglen == 3) {
      blink::WebRect rect = context.render_view_impl()->windowRect();
      mouse_event_x = rect.x + rect.width / 2;
      mouse_event_y = rect.y + rect.height / 2;
    } else {
      if (arglen != 5 ||
          !args[3]->IsNumber() ||
          !args[4]->IsNumber()) {
        args.GetReturnValue().Set(false);
        return;
      }

      mouse_event_x = args[3]->IntegerValue() *
          context.web_view()->pageScaleFactor();
      mouse_event_y = args[4]->IntegerValue() *
          context.web_view()->pageScaleFactor();
    }

    // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
    // progress, we will leak the callback and context. This needs to be fixed,
    // somehow.
    context.render_view_impl()->BeginSmoothScroll(
        scroll_down,
        base::Bind(&OnSmoothScrollCompleted,
                   callback_and_context),
        pixels_to_scroll,
        mouse_event_x,
        mouse_event_y);

    args.GetReturnValue().Set(true);
  }

  static void BeginPinch(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuBenchmarkingContext context;
    if (!context.Init(false))
      return;

    int arglen = args.Length();
    if (arglen < 5 ||
        !args[0]->IsBoolean() ||
        !args[1]->IsNumber() ||
        !args[2]->IsNumber() ||
        !args[3]->IsNumber() ||
        !args[4]->IsFunction()) {
      args.GetReturnValue().Set(false);
      return;
    }

    bool zoom_in = args[0]->BooleanValue();
    int pixels_to_move = args[1]->IntegerValue();
    int anchor_x = args[2]->IntegerValue();
    int anchor_y = args[3]->IntegerValue();

    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>::Cast(args[4]);

    scoped_refptr<CallbackAndContext> callback_and_context =
        new CallbackAndContext(args.GetIsolate(),
                               callback_local,
                               context.web_frame()->mainWorldScriptContext());


    // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
    // progress, we will leak the callback and context. This needs to be fixed,
    // somehow.
    context.render_view_impl()->BeginPinch(
        zoom_in,
        pixels_to_move,
        anchor_x,
        anchor_y,
        base::Bind(&OnSmoothScrollCompleted,
                   callback_and_context));

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
    GpuBenchmarkingContext context;
    if (!context.Init(false))
      return;

    if (!args[0]->IsFunction())
      return;

    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>::Cast(args[0]);

    scoped_refptr<CallbackAndContext> callback_and_context =
        new CallbackAndContext(args.GetIsolate(),
                               callback_local,
                               context.web_frame()->mainWorldScriptContext());

    context.render_view_impl()->GetWindowSnapshot(
        base::Bind(&OnSnapshotCompleted, callback_and_context));
  }

  static void ClearImageCache(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    WebImageCache::clear();
  }

  static void OnMicroBenchmarkCompleted(
      CallbackAndContext* callback_and_context,
      scoped_ptr<base::Value> result) {
    v8::HandleScope scope(callback_and_context->isolate());
    v8::Handle<v8::Context> context = callback_and_context->GetContext();
    v8::Context::Scope context_scope(context);
    WebFrame* frame = WebFrame::frameForContext(context);
    if (frame) {
      scoped_ptr<V8ValueConverter> converter =
          make_scoped_ptr(V8ValueConverter::create());
      v8::Handle<v8::Value> value = converter->ToV8Value(result.get(), context);
      v8::Handle<v8::Value> argv[] = { value };

      frame->callFunctionEvenIfScriptDisabled(
          callback_and_context->GetCallback(), v8::Object::New(), 1, argv);
    }
  }

  static void RunMicroBenchmark(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuBenchmarkingContext context;
    if (!context.Init(true)) {
      args.GetReturnValue().Set(false);
      return;
    }

    if (args.Length() != 3 ||
        !args[0]->IsString() ||
        !args[1]->IsFunction() ||
        !args[2]->IsObject()) {
      args.GetReturnValue().Set(false);
      return;
    }

    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>::Cast(args[1]);

    scoped_refptr<CallbackAndContext> callback_and_context =
        new CallbackAndContext(args.GetIsolate(),
                               callback_local,
                               context.web_frame()->mainWorldScriptContext());

    scoped_ptr<V8ValueConverter> converter =
        make_scoped_ptr(V8ValueConverter::create());
    v8::Handle<v8::Context> v8_context = callback_and_context->GetContext();
    scoped_ptr<base::Value> value =
        make_scoped_ptr(converter->FromV8Value(args[2], v8_context));

    v8::String::Utf8Value benchmark(args[0]);
    DCHECK(*benchmark);
    args.GetReturnValue().Set(context.compositor()->ScheduleMicroBenchmark(
        std::string(*benchmark),
        value.Pass(),
        base::Bind(&OnMicroBenchmarkCompleted, callback_and_context)));
  }

  static void HasGpuProcess(const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuChannelHost* gpu_channel = RenderThreadImpl::current()->GetGpuChannel();
    args.GetReturnValue().Set(!!gpu_channel);
  }
};

v8::Extension* GpuBenchmarkingExtension::Get() {
  return new GpuBenchmarkingWrapper();
}

}  // namespace content
