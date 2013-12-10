// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/modules/console.h"

#include <iostream>

#include "base/strings/string_util.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/per_isolate_data.h"
#include "gin/public/wrapper_info.h"

using v8::ObjectTemplate;

namespace gin {

namespace {

void Log(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Arguments args(info);

  std::vector<std::string> messages;
  if (!args.GetRemaining(&messages))
    return args.ThrowTypeError("Expected strings.");

  std::cout << JoinString(messages, ' ') << std::endl;
}

WrapperInfo g_wrapper_info = { kEmbedderNativeGin };

}  // namespace

const char Console::kModuleName[] = "console";

v8::Local<ObjectTemplate> Console::GetTemplate(v8::Isolate* isolate) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  v8::Local<ObjectTemplate> templ = data->GetObjectTemplate(&g_wrapper_info);
  if (templ.IsEmpty()) {
    templ = ObjectTemplate::New();
    templ->Set(StringToSymbol(isolate, "log"),
               v8::FunctionTemplate::New(isolate, Log));
    data->SetObjectTemplate(&g_wrapper_info, templ);
  }
  return templ;
}

}  // namespace gin
