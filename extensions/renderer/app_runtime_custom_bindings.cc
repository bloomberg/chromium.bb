// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/app_runtime_custom_bindings.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebBlob.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"

using blink::WebBlob;
using blink::WebSerializedScriptValue;
using blink::WebString;

namespace {

void DeserializeString(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK(args.Length() == 1);
  DCHECK(args[0]->IsString());

  std::string data_v8(*v8::String::Utf8Value(args[0]));
  WebString data_webstring = WebString::fromUTF8(data_v8);
  WebSerializedScriptValue serialized =
      WebSerializedScriptValue::fromString(data_webstring);
  args.GetReturnValue().Set(serialized.deserialize());
}

void SerializeToString(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK(args.Length() == 1);
  WebSerializedScriptValue data = WebSerializedScriptValue::serialize(args[0]);
  WebString data_webstring = data.toString();

  std::string v = std::string(data_webstring.utf8());
  args.GetReturnValue().Set(
      v8::String::NewFromUtf8(args.GetIsolate(), v.c_str()));
}

void CreateBlob(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DCHECK(args.Length() == 2);
  DCHECK(args[0]->IsString());
  DCHECK(args[1]->IsNumber());

  std::string blob_file_path(*v8::String::Utf8Value(args[0]));
  std::string blob_length_string(*v8::String::Utf8Value(args[1]));
  int64 blob_length = 0;
  DCHECK(base::StringToInt64(blob_length_string, &blob_length));
  blink::WebBlob web_blob =
      WebBlob::createFromFile(WebString::fromUTF8(blob_file_path), blob_length);
  args.GetReturnValue().Set(
      web_blob.toV8Value(args.Holder(), args.GetIsolate()));
}

}  // namespace

namespace extensions {

AppRuntimeCustomBindings::AppRuntimeCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("DeserializeString", base::Bind(&DeserializeString));
  RouteFunction("SerializeToString", base::Bind(&SerializeToString));
  RouteFunction("CreateBlob", base::Bind(&CreateBlob));
}

}  // namespace extensions
