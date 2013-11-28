// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/skia_benchmarking_extension.h"

#include "base/base64.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/resources/picture.h"
#include "content/public/renderer/v8_value_converter.h"
#include "skia/ext/benchmarking_canvas.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/src/utils/debugger/SkDebugCanvas.h"
#include "third_party/skia/src/utils/debugger/SkDrawCommand.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "v8/include/v8.h"

using blink::WebFrame;

namespace {

const char kSkiaBenchmarkingExtensionName[] = "v8/SkiaBenchmarking";

static scoped_ptr<base::Value> ParsePictureArg(v8::Isolate* isolate,
                                               v8::Handle<v8::Value> arg) {
  scoped_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  return scoped_ptr<base::Value>(
      converter->FromV8Value(arg, isolate->GetCurrentContext()));
}

static scoped_refptr<cc::Picture> ParsePictureStr(v8::Isolate* isolate,
                                                  v8::Handle<v8::Value> arg) {
  scoped_ptr<base::Value> picture_value = ParsePictureArg(isolate, arg);
  if (!picture_value)
    return NULL;
  return cc::Picture::CreateFromSkpValue(picture_value.get());
}

static scoped_refptr<cc::Picture> ParsePictureHash(v8::Isolate* isolate,
                                                   v8::Handle<v8::Value> arg) {
  scoped_ptr<base::Value> picture_value = ParsePictureArg(isolate, arg);
  if (!picture_value)
    return NULL;
  return cc::Picture::CreateFromValue(picture_value.get());
}

class SkiaBenchmarkingWrapper : public v8::Extension {
 public:
  SkiaBenchmarkingWrapper() :
      v8::Extension(kSkiaBenchmarkingExtensionName,
        "if (typeof(chrome) == 'undefined') {"
        "  chrome = {};"
        "};"
        "if (typeof(chrome.skiaBenchmarking) == 'undefined') {"
        "  chrome.skiaBenchmarking = {};"
        "};"
        "chrome.skiaBenchmarking.rasterize = function(picture, params) {"
        "  /* "
        "     Rasterizes a Picture JSON-encoded by cc::Picture::AsValue()."
        "     @param {Object} picture A json-encoded cc::Picture."
        "     @param {"
        "                 'scale':    {Number},"
        "                 'stop':     {Number},"
        "                 'overdraw': {Boolean},"
        "                 'clip':     [Number, Number, Number, Number]"
        "     } (optional) Rasterization parameters."
        "     @returns {"
        "                 'width':    {Number},"
        "                 'height':   {Number},"
        "                 'data':     {ArrayBuffer}"
        "     }"
        "     @returns undefined if the arguments are invalid or the picture"
        "                        version is not supported."
        "   */"
        "  native function Rasterize();"
        "  return Rasterize(picture, params);"
        "};"
        "chrome.skiaBenchmarking.getOps = function(picture) {"
        "  /* "
        "     Extracts the Skia draw commands from a JSON-encoded cc::Picture"
        "     @param {Object} picture A json-encoded cc::Picture."
        "     @returns [{ 'cmd': {String}, 'info': [String, ...] }, ...]"
        "     @returns undefined if the arguments are invalid or the picture"
        "                        version is not supported."
        "   */"
        "  native function GetOps();"
        "  return GetOps(picture);"
        "};"
        "chrome.skiaBenchmarking.getOpTimings = function(picture) {"
        "  /* "
        "     Returns timing information for the given picture."
        "     @param {Object} picture A json-encoded cc::Picture."
        "     @returns { 'total_time': {Number}, 'cmd_times': [Number, ...] }"
        "     @returns undefined if the arguments are invalid or the picture"
        "                        version is not supported."
        "   */"
        "  native function GetOpTimings();"
        "  return GetOpTimings(picture);"
        "};"
        "chrome.skiaBenchmarking.getInfo = function(picture) {"
        "  /* "
        "     Returns meta information for the given picture."
        "     @param {Object} picture A base64 encoded SKP."
        "     @returns { 'width': {Number}, 'height': {Number} }"
        "     @returns undefined if the picture version is not supported."
        "   */"
        "  native function GetInfo();"
        "  return GetInfo(picture);"
        "};"
        ) {
      content::SkiaBenchmarkingExtension::InitSkGraphics();
  }

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(v8::String::NewFromUtf8(isolate, "Rasterize")))
      return v8::FunctionTemplate::New(isolate, Rasterize);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "GetOps")))
      return v8::FunctionTemplate::New(isolate, GetOps);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "GetOpTimings")))
      return v8::FunctionTemplate::New(isolate, GetOpTimings);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "GetInfo")))
      return v8::FunctionTemplate::New(isolate, GetInfo);

    return v8::Handle<v8::FunctionTemplate>();
  }

  static void Rasterize(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() < 1)
      return;

    v8::Isolate* isolate = args.GetIsolate();
    scoped_refptr<cc::Picture> picture = ParsePictureHash(isolate, args[0]);
    if (!picture.get())
      return;

    double scale = 1.0;
    gfx::Rect clip_rect(picture->LayerRect());
    int stop_index = -1;
    bool overdraw = false;

    if (args.Length() > 1) {
      scoped_ptr<content::V8ValueConverter> converter(
          content::V8ValueConverter::create());
      scoped_ptr<base::Value> params_value(
          converter->FromV8Value(args[1], isolate->GetCurrentContext()));

      const base::DictionaryValue* params_dict = NULL;
      if (params_value.get() && params_value->GetAsDictionary(&params_dict)) {
        params_dict->GetDouble("scale", &scale);
        params_dict->GetInteger("stop", &stop_index);
        params_dict->GetBoolean("overdraw", &overdraw);

        const base::Value* clip_value = NULL;
        if (params_dict->Get("clip", &clip_value))
          cc::MathUtil::FromValue(clip_value, &clip_rect);
      }
    }

    gfx::RectF clip(clip_rect);
    clip.Intersect(picture->LayerRect());
    clip.Scale(scale);
    gfx::Rect snapped_clip = gfx::ToEnclosingRect(clip);

    const int kMaxBitmapSize = 4096;
    if (snapped_clip.width() > kMaxBitmapSize
        || snapped_clip.height() > kMaxBitmapSize)
      return;

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, snapped_clip.width(),
                     snapped_clip.height());
    if (!bitmap.allocPixels())
      return;
    bitmap.eraseARGB(0, 0, 0, 0);

    SkCanvas canvas(bitmap);
    canvas.translate(SkFloatToScalar(-clip.x()),
                     SkFloatToScalar(-clip.y()));
    canvas.clipRect(gfx::RectToSkRect(snapped_clip));
    canvas.scale(scale, scale);
    canvas.translate(picture->LayerRect().x(),
                     picture->LayerRect().y());

    // First, build a debug canvas for the given picture.
    SkDebugCanvas debug_canvas(picture->LayerRect().width(),
                               picture->LayerRect().height());
    picture->Replay(&debug_canvas);

    // Raster the requested command subset into the bitmap-backed canvas.
    int last_index = debug_canvas.getSize() - 1;
    if (last_index >= 0) {
        debug_canvas.setOverdrawViz(overdraw);
        debug_canvas.drawTo(&canvas, stop_index < 0
                            ? last_index
                            : std::min(last_index, stop_index));
    }

    blink::WebArrayBuffer buffer =
        blink::WebArrayBuffer::create(bitmap.getSize(), 1);
    uint32* packed_pixels = reinterpret_cast<uint32*>(bitmap.getPixels());
    uint8* buffer_pixels = reinterpret_cast<uint8*>(buffer.data());
    // Swizzle from native Skia format to RGBA as we copy out.
    for (size_t i = 0; i < bitmap.getSize(); i += 4) {
        uint32 c = packed_pixels[i >> 2];
        buffer_pixels[i]     = SkGetPackedR32(c);
        buffer_pixels[i + 1] = SkGetPackedG32(c);
        buffer_pixels[i + 2] = SkGetPackedB32(c);
        buffer_pixels[i + 3] = SkGetPackedA32(c);
    }

    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8::String::NewFromUtf8(isolate, "width"),
                v8::Number::New(snapped_clip.width()));
    result->Set(v8::String::NewFromUtf8(isolate, "height"),
                v8::Number::New(snapped_clip.height()));
    result->Set(v8::String::NewFromUtf8(isolate, "data"), buffer.toV8Value());

    args.GetReturnValue().Set(result);
  }

  static void GetOps(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1)
      return;

    v8::Isolate* isolate = args.GetIsolate();
    scoped_refptr<cc::Picture> picture = ParsePictureHash(isolate, args[0]);
    if (!picture.get())
      return;

    gfx::Rect bounds = picture->LayerRect();
    SkDebugCanvas canvas(bounds.width(), bounds.height());
    picture->Replay(&canvas);

    v8::Local<v8::Array> result = v8::Array::New(isolate, canvas.getSize());
    for (int i = 0; i < canvas.getSize(); ++i) {
      DrawType cmd_type = canvas.getDrawCommandAt(i)->getType();
      v8::Handle<v8::Object> cmd = v8::Object::New();
      cmd->Set(v8::String::NewFromUtf8(isolate, "cmd_type"),
               v8::Integer::New(isolate, cmd_type));
      cmd->Set(v8::String::NewFromUtf8(isolate, "cmd_string"),
               v8::String::NewFromUtf8(
                   isolate, SkDrawCommand::GetCommandString(cmd_type)));

      SkTDArray<SkString*>* info = canvas.getCommandInfo(i);
      DCHECK(info);

      v8::Local<v8::Array> v8_info = v8::Array::New(isolate, info->count());
      for (int j = 0; j < info->count(); ++j) {
        const SkString* info_str = (*info)[j];
        DCHECK(info_str);
        v8_info->Set(j, v8::String::NewFromUtf8(isolate, info_str->c_str()));
      }

      cmd->Set(v8::String::NewFromUtf8(isolate, "info"), v8_info);

      result->Set(i, cmd);
    }

    args.GetReturnValue().Set(result);
  }

  static void GetOpTimings(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1)
      return;

    v8::Isolate* isolate = args.GetIsolate();
    scoped_refptr<cc::Picture> picture = ParsePictureHash(isolate, args[0]);
    if (!picture.get())
      return;

    gfx::Rect bounds = picture->LayerRect();

    // Measure the total time by drawing straight into a bitmap-backed canvas.
    skia::RefPtr<SkBaseDevice> device = skia::AdoptRef(
        SkNEW_ARGS(SkBitmapDevice,
             (SkBitmap::kARGB_8888_Config, bounds.width(), bounds.height())));
    SkCanvas bitmap_canvas(device.get());
    bitmap_canvas.clear(SK_ColorTRANSPARENT);
    base::TimeTicks t0 = base::TimeTicks::HighResNow();
    picture->Replay(&bitmap_canvas);
    base::TimeDelta total_time = base::TimeTicks::HighResNow() - t0;

    // Gather per-op timing info by drawing into a BenchmarkingCanvas.
    skia::BenchmarkingCanvas benchmarking_canvas(bounds.width(),
                                                 bounds.height());
    picture->Replay(&benchmarking_canvas);

    v8::Local<v8::Array> op_times =
        v8::Array::New(isolate, benchmarking_canvas.CommandCount());
    for (size_t i = 0; i < benchmarking_canvas.CommandCount(); ++i)
        op_times->Set(i, v8::Number::New(benchmarking_canvas.GetTime(i)));

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->Set(v8::String::NewFromUtf8(isolate, "total_time"),
                v8::Number::New(total_time.InMillisecondsF()));
    result->Set(v8::String::NewFromUtf8(isolate, "cmd_times"), op_times);

    args.GetReturnValue().Set(result);
  }

  static void GetInfo(const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1)
      return;

    v8::Isolate* isolate = args.GetIsolate();
    scoped_refptr<cc::Picture> picture = ParsePictureStr(isolate, args[0]);
    if (!picture.get())
      return;

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->Set(v8::String::NewFromUtf8(isolate, "width"),
                v8::Number::New(picture->LayerRect().width()));
    result->Set(v8::String::NewFromUtf8(isolate, "height"),
                v8::Number::New(picture->LayerRect().height()));

    args.GetReturnValue().Set(result);
  }
};

} // namespace

namespace content {

v8::Extension* SkiaBenchmarkingExtension::Get() {
  return new SkiaBenchmarkingWrapper();
}

void SkiaBenchmarkingExtension::InitSkGraphics() {
    // Always call on the main render thread.
    // Does not need to be thread-safe, as long as the above holds.
    // FIXME: remove this after Skia updates SkGraphics::Init() to be
    //        thread-safe and idempotent.
    static bool skia_initialized = false;
    if (!skia_initialized) {
      SkGraphics::Init();
      skia_initialized = true;
    }
}

} // namespace content
