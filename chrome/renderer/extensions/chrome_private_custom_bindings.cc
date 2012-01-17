// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_private_custom_bindings.h"

#include <algorithm>
#include <string>
#include <vector>

#include "chrome/common/extensions/extension_action.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "grit/renderer_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "v8/include/v8.h"
#include "webkit/glue/webkit_glue.h"

namespace extensions {

ChromePrivateCustomBindings::ChromePrivateCustomBindings(
    int dependency_count,
    const char** dependencies,
    ExtensionDispatcher* extension_dispatcher)
    : ChromeV8Extension(
          "extensions/chrome_private_custom_bindings.js",
          IDR_CHROME_PRIVATE_CUSTOM_BINDINGS_JS,
          dependency_count,
          dependencies,
          extension_dispatcher) {}

// static
v8::Handle<v8::Value> ChromePrivateCustomBindings::DecodeJPEG(
    const v8::Arguments& args) {
  static const char* kAllowedIds[] = {
      "haiffjcadagjlijoggckpgfnoeiflnem",
      "gnedhmakppccajfpfiihfcdlnpgomkcf",
      "fjcibdnjlbfnbfdjneajpipnlcppleek",
      "oflbaaikkabfdfkimeclgkackhdkpnip"  // Testing extension.
  };
  const std::vector<std::string> allowed_ids(
      kAllowedIds, kAllowedIds + arraysize(kAllowedIds));

  ChromePrivateCustomBindings* v8_extension =
      GetFromArguments<ChromePrivateCustomBindings>(args);
  const ::Extension* extension =
      v8_extension->GetExtensionForCurrentRenderView();
  if (!extension)
    return v8::Undefined();
  if (allowed_ids.end() == std::find(
        allowed_ids.begin(), allowed_ids.end(), extension->id())) {
    return v8::Undefined();
  }

  DCHECK(args.Length() == 1);
  DCHECK(args[0]->IsArray());
  v8::Local<v8::Object> jpeg_array = args[0]->ToObject();
  size_t jpeg_length =
      jpeg_array->Get(v8::String::New("length"))->Int32Value();

  // Put input JPEG array into string for DecodeImage().
  std::string jpeg_array_string;
  jpeg_array_string.reserve(jpeg_length);

  // Unfortunately we cannot request continuous backing store of the
  // |jpeg_array| object as it might not have one. So we make
  // element copy here.
  // Note(mnaganov): If it is not fast enough
  // (and main constraints might be in repetition of v8 API calls),
  // change the argument type from Array to String and use
  // String::Write().
  // Note(vitalyr): Another option is to use Int8Array for inputs and
  // Int32Array for output.
  for (size_t i = 0; i != jpeg_length; ++i) {
    jpeg_array_string.push_back(
        jpeg_array->Get(v8::Integer::New(i))->Int32Value());
  }

  // Decode and verify resulting image metrics.
  SkBitmap bitmap;
  if (!webkit_glue::DecodeImage(jpeg_array_string, &bitmap))
    return v8::Undefined();
  if (bitmap.config() != SkBitmap::kARGB_8888_Config)
    return v8::Undefined();
  const int width = bitmap.width();
  const int height = bitmap.height();
  SkAutoLockPixels lockpixels(bitmap);
  const uint32_t* pixels = static_cast<uint32_t*>(bitmap.getPixels());
  if (!pixels)
    return v8::Undefined();

  // Compose output array. This API call only accepts kARGB_8888_Config images
  // so we rely on each pixel occupying 4 bytes.
  // Note(mnaganov): to speed this up, you may use backing store
  // technique from CreateExternalArray() of v8/src/d8.cc.
  v8::Local<v8::Array> bitmap_array(v8::Array::New(width * height));
  for (int i = 0; i != width * height; ++i) {
    bitmap_array->Set(v8::Integer::New(i),
                      v8::Integer::New(pixels[i] & 0xFFFFFF));
  }
  return bitmap_array;
}

v8::Handle<v8::FunctionTemplate> ChromePrivateCustomBindings::GetNativeFunction(
    v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("DecodeJPEG")))
    return v8::FunctionTemplate::New(DecodeJPEG, v8::External::New(this));

  return ChromeV8Extension::GetNativeFunction(name);
}

}  // namespace extensions
