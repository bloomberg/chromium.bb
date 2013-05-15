// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/skia_benchmarking_extension.h"

#include "cc/resources/picture.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebArrayBuffer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/gfx/rect_conversions.h"
#include "v8/include/v8.h"

namespace {

const char kSkiaBenchmarkingExtensionName[] = "v8/SkiaBenchmarking";

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
        "chrome.skiaBenchmarking.rasterize = function(picture, scale, rect) {"
        "  /* "
        "     Rasterizes a base64-encoded Picture."
        "     @param {String} picture Base64-encoded Picture."
        "     @param {Number} scale (optional) Rendering scale."
        "     @param [Number, Number, Number, Number] clip_rect (optional)."
        "     @returns { 'width': {Number}, 'height': {Number},"
        "                'data': {ArrayBuffer} }"
        "   */"
        "  native function Rasterize();"
        "  return Rasterize(picture, scale, rect);"
        "};"
        ) {
      content::SkiaBenchmarkingExtension::InitSkGraphics();
  }

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(v8::String::New("Rasterize")))
      return v8::FunctionTemplate::New(Rasterize);

    return v8::Handle<v8::FunctionTemplate>();
  }

  static v8::Handle<v8::Value> Rasterize(const v8::Arguments& args) {
    if (args.Length() < 1)
      return v8::Undefined();

    v8::String::AsciiValue base64_picture(args[0]);
    scoped_refptr<cc::Picture> picture =
        cc::Picture::CreateFromBase64String(*base64_picture);
    if (!picture)
      return v8::Undefined();

    float scale = 1.0f;
    if (args.Length() > 1 && args[1]->IsNumber())
      scale = std::max(0.01, std::min(100.0, args[1]->NumberValue()));

    gfx::RectF clip(picture->LayerRect());
    if (args.Length() > 2 && args[2]->IsArray()) {
      v8::Array* a = v8::Array::Cast(*args[2]);
      if (a->Length() == 4
          && a->Get(0)->IsNumber()
          && a->Get(1)->IsNumber()
          && a->Get(2)->IsNumber()
          && a->Get(3)->IsNumber()) {
        clip.SetRect(a->Get(0)->NumberValue(), a->Get(1)->NumberValue(),
                     a->Get(2)->NumberValue(), a->Get(3)->NumberValue());
        clip.Intersect(picture->LayerRect());
      }
    }

    // cc::Picture::Raster() clips before scaling, so the clip needs to be
    // scaled explicitly.
    clip.Scale(scale);
    gfx::Rect snapped_clip = gfx::ToEnclosingRect(clip);

    const int kMaxBitmapSize = 4096;
    if (snapped_clip.width() > kMaxBitmapSize
        || snapped_clip.height() > kMaxBitmapSize)
      return v8::Undefined();

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, snapped_clip.width(),
                     snapped_clip.height());
    if (!bitmap.allocPixels())
      return v8::Undefined();

    bitmap.eraseARGB(0, 0, 0, 0);
    SkCanvas canvas(bitmap);
    canvas.translate(SkFloatToScalar(-clip.x()),
                     SkFloatToScalar(-clip.y()));
    picture->Raster(&canvas, snapped_clip, scale, true);

    WebKit::WebArrayBuffer buffer =
        WebKit::WebArrayBuffer::create(bitmap.getSize(), 1);
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
    result->Set(v8::String::New("width"),
                v8::Number::New(snapped_clip.width()));
    result->Set(v8::String::New("height"),
                v8::Number::New(snapped_clip.height()));
    result->Set(v8::String::New("data"), buffer.toV8Value());

    return result;
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
