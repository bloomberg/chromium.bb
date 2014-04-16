// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/bindings/js/unicode.h"

#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/wrapper_info.h"

namespace mojo {
namespace js {

namespace {

v8::Handle<v8::Value> DecodeUtf8String(const gin::Arguments& args,
                                       const gin::ArrayBufferView& buffer) {
  assert(static_cast<int>(buffer.num_bytes()) >= 0);
  return v8::String::NewFromUtf8(args.isolate(),
                                 reinterpret_cast<char*>(buffer.bytes()),
                                 v8::String::kNormalString,
                                 static_cast<int>(buffer.num_bytes()));
}

int EncodeUtf8String(const gin::Arguments& args,
                     v8::Handle<v8::Value> str_value,
                     const gin::ArrayBufferView& buffer) {
  assert(static_cast<int>(buffer.num_bytes()) >= 0);
  v8::Handle<v8::String> str = str_value->ToString();
  int num_bytes = str->WriteUtf8(
      reinterpret_cast<char*>(buffer.bytes()),
      static_cast<int>(buffer.num_bytes()),
      NULL,
      v8::String::NO_NULL_TERMINATION | v8::String::REPLACE_INVALID_UTF8);
  return num_bytes;
}

int Utf8Length(v8::Handle<v8::Value> str_value) {
  return str_value->ToString()->Utf8Length();
}

gin::WrapperInfo g_wrapper_info = { gin::kEmbedderNativeGin };

}  // namespace

const char Unicode::kModuleName[] = "mojo/bindings/js/unicode";

v8::Local<v8::Value> Unicode::GetModule(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_wrapper_info);

  if (templ.IsEmpty()) {
    templ = gin::ObjectTemplateBuilder(isolate)
                .SetMethod("decodeUtf8String", DecodeUtf8String)
                .SetMethod("encodeUtf8String", EncodeUtf8String)
                .SetMethod("utf8Length", Utf8Length)
                .Build();

    data->SetObjectTemplate(&g_wrapper_info, templ);
  }

  return templ->NewInstance();
}

}  // namespace js
}  // namespace mojo
