// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/utils_native_handler.h"

#include "base/strings/stringprintf.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "third_party/WebKit/public/web/WebScopedMicrotaskSuppression.h"

namespace extensions {

UtilsNativeHandler::UtilsNativeHandler(ChromeV8Context* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("createClassWrapper",
      base::Bind(&UtilsNativeHandler::CreateClassWrapper,
                 base::Unretained(this)));
}

UtilsNativeHandler::~UtilsNativeHandler() {}

void UtilsNativeHandler::CreateClassWrapper(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsString());
  std::string name = *v8::String::Utf8Value(args[0]);
  CHECK(args[1]->IsObject());
  v8::Local<v8::Object> obj = args[1].As<v8::Object>();

  v8::HandleScope handle_scope(GetIsolate());
  // TODO(fsamuel): Consider moving the source wrapping to ModuleSystem.
  v8::Handle<v8::String> source = v8::String::NewFromUtf8(
      GetIsolate(),
      base::StringPrintf(
          "(function($Object, $Function, privates, cls) {"
          "'use strict';\n"
          "  return function %s() {\n"
          "  var privateObj = $Object.create(cls.prototype);\n"
          "  $Function.apply(cls, privateObj, arguments);\n"
          "  privateObj.wrapper = this;\n"
          "  privates(this).impl = privateObj;\n"
          "}})", name.c_str()).c_str());
  v8::Handle<v8::Value> func_as_value =
      context()->module_system()->RunString(source,
          v8::String::NewFromUtf8(GetIsolate(), name.c_str()));
  if (func_as_value.IsEmpty() || func_as_value->IsUndefined()) {
    args.GetReturnValue().SetUndefined();
    return;
  }

  // TODO(fsamuel): Move privates from ModuleSystem to a shared location.
  v8::Handle<v8::Object> natives(context()->module_system()->NewInstance());
  CHECK(!natives.IsEmpty());  // this can happen if v8 has issues
  v8::Handle<v8::Function> func = func_as_value.As<v8::Function>();
  v8::Handle<v8::Value> func_args[] = {
      context()->safe_builtins()->GetObjekt(),
      context()->safe_builtins()->GetFunction(),
      natives->Get(v8::String::NewFromUtf8(
          GetIsolate(), "privates", v8::String::kInternalizedString)),
      obj
  };
  v8::Local<v8::Value> result;
  {
    v8::TryCatch try_catch;
    try_catch.SetCaptureMessage(true);
    result = context()->CallFunction(func, arraysize(func_args), func_args);
    if (try_catch.HasCaught()) {
      args.GetReturnValue().SetUndefined();
      return;
    }
  }
  args.GetReturnValue().Set(result);
}

}  // namespace extensions
