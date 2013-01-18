// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/app_runtime_custom_bindings.h"

#include "base/string_number_conversions.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBlob.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"

using WebKit::WebBlob;
using WebKit::WebSerializedScriptValue;
using WebKit::WebString;

namespace {

v8::Handle<v8::Value> DeserializeString(const v8::Arguments &args) {
  DCHECK(args.Length() == 1);
  DCHECK(args[0]->IsString());

  std::string data_v8(*v8::String::Utf8Value(args[0]));
  WebString data_webstring = WebString::fromUTF8(data_v8);
  WebSerializedScriptValue serialized =
      WebSerializedScriptValue::fromString(data_webstring);
  return serialized.deserialize();
}

v8::Handle<v8::Value> SerializeToString(const v8::Arguments &args) {
  DCHECK(args.Length() == 1);
  WebSerializedScriptValue data =
      WebSerializedScriptValue::serialize(args[0]);
  WebString data_webstring = data.toString();

  std::string v = std::string(data_webstring.utf8());
  return v8::String::New(v.c_str());
}

v8::Handle<v8::Value> CreateBlob(const v8::Arguments &args) {
  DCHECK(args.Length() == 2);
  DCHECK(args[0]->IsString());
  DCHECK(args[1]->IsNumber());

  std::string blob_file_path(*v8::String::Utf8Value(args[0]));
  std::string blob_length_string(*v8::String::Utf8Value(args[1]));
  int64 blob_length = 0;
  DCHECK(base::StringToInt64(blob_length_string, &blob_length));
  WebKit::WebBlob web_blob = WebBlob::createFromFile(
      WebString::fromUTF8(blob_file_path), blob_length);
  return web_blob.toV8Value();
}

}  // namespace

namespace extensions {

AppRuntimeCustomBindings::AppRuntimeCustomBindings()
    : ChromeV8Extension(NULL) {
  RouteStaticFunction("DeserializeString", &DeserializeString);
  RouteStaticFunction("SerializeToString", &SerializeToString);
  RouteStaticFunction("CreateBlob", &CreateBlob);
}

}  // namespace extensions
