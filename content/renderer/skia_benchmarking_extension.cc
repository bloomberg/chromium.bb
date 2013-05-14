// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/skia_benchmarking_extension.h"

#include "cc/resources/picture.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebArrayBuffer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkGraphics.h"
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
        "chrome.skiaBenchmarking.rasterize = function(picture) {"
        "  /* "
        "     Rasterizes a base64-encoded Picture."
        "     @param {String} picture Base64-encoded Picture. "
        "     @returns { 'width': {Number}, 'height': {Number},"
        "                'data': {ArrayBuffer} }"
        "   */"
        "  native function Rasterize();"
        "  return Rasterize(picture);"
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
    if (args.Length() != 1)
      return v8::Undefined();

    v8::String::AsciiValue base64_picture(args[0]);
    scoped_refptr<cc::Picture> picture =
        cc::Picture::CreateFromBase64String(*base64_picture);
    if (!picture)
      return v8::Undefined();

    gfx::Rect rect = picture->LayerRect();
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height());
    if (!bitmap.allocPixels())
      return v8::Undefined();

    bitmap.eraseARGB(0, 0, 0, 0);
    SkCanvas canvas(bitmap);
    picture->Raster(&canvas, rect, 1, true);

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
    result->Set(v8::String::New("width"), v8::Number::New(rect.width()));
    result->Set(v8::String::New("height"), v8::Number::New(rect.height()));
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
