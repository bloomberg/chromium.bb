// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/print_native_handler.h"

#include <string>

#include "base/bind.h"
#include "base/strings/string_util.h"

namespace extensions {

PrintNativeHandler::PrintNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("Print",
                base::Bind(&PrintNativeHandler::Print, base::Unretained(this)));
}

void PrintNativeHandler::Print(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 1)
    return;

  std::vector<std::string> components;
  for (int i = 0; i < args.Length(); ++i)
    components.push_back(*v8::String::Utf8Value(args[i]->ToString()));

  LOG(ERROR) << JoinString(components, ',');
}

}  // namespace extensions
