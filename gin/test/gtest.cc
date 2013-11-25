// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/test/gtest.h"

#include "base/bind.h"
#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/function_template.h"
#include "gin/per_isolate_data.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {

namespace {

void Fail(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Arguments args(info);

  std::string description;
  if (!args.GetNext(&description))
    return args.ThrowError();

  FAIL() << description;
}

void ExpectTrue(bool condition, const std::string& description) {
  EXPECT_TRUE(condition) << description;
}

void ExpectFalse(bool condition, const std::string& description) {
  EXPECT_FALSE(condition) << description;
}

void ExpectEqual(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Arguments args(info);

  std::string description;
  if (!ConvertFromV8(info[2], &description))
    return args.ThrowTypeError("Expected description.");

  EXPECT_TRUE(info[0]->StrictEquals(info[1])) << description;
}

WrapperInfo g_wrapper_info = { kEmbedderNativeGin };

}  // namespace

const char GTest::kModuleName[] = "gtest";

v8::Local<v8::ObjectTemplate> GTest::GetTemplate(v8::Isolate* isolate) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ =
      data->GetObjectTemplate(&g_wrapper_info);
  if (templ.IsEmpty()) {
    templ = v8::ObjectTemplate::New();
    templ->Set(StringToSymbol(isolate, "fail"),
               v8::FunctionTemplate::New(Fail));
    templ->Set(StringToSymbol(isolate, "expectTrue"),
               CreateFunctionTempate(isolate, base::Bind(ExpectTrue)));
    templ->Set(StringToSymbol(isolate, "expectFalse"),
               CreateFunctionTempate(isolate, base::Bind(ExpectFalse)));
    templ->Set(StringToSymbol(isolate, "expectEqual"),
               v8::FunctionTemplate::New(ExpectEqual));
    data->SetObjectTemplate(&g_wrapper_info, templ);
  }
  return templ;
}

}  // namespace gin
