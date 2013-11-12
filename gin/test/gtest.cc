// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/test/gtest.h"

#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/per_isolate_data.h"
#include "gin/wrapper_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {

namespace {

void ExpectTrue(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Arguments args(info);

  bool value = false;
  std::string description;

  if (!args.GetNext(&value) ||
      !args.GetNext(&description)) {
    return args.ThrowError();
  }

  EXPECT_TRUE(value) << description;
}

void ExpectFalse(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Arguments args(info);

  bool value = false;
  std::string description;

  if (!args.GetNext(&value) ||
      !args.GetNext(&description)) {
    return args.ThrowError();
  }

  EXPECT_FALSE(value) << description;
}

void ExpectEqual(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Arguments args(info);

  std::string description;
  if (!ConvertFromV8(info[2], &description))
    return args.ThrowTypeError("Expected description.");

  EXPECT_TRUE(info[0]->StrictEquals(info[1])) << description;
}

WrapperInfo g_gtest_wrapper_info = {};

}

v8::Local<v8::ObjectTemplate> GetGTestTemplate(v8::Isolate* isolate) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_gtest_wrapper_info);
  if (templ.IsEmpty()) {
    templ = v8::ObjectTemplate::New();
    templ->Set(StringToSymbol(isolate, "expectTrue"),
               v8::FunctionTemplate::New(ExpectTrue));
    templ->Set(StringToSymbol(isolate, "expectFalse"),
               v8::FunctionTemplate::New(ExpectFalse));
    templ->Set(StringToSymbol(isolate, "expectEqual"),
               v8::FunctionTemplate::New(ExpectEqual));
    data->SetObjectTemplate(&g_gtest_wrapper_info, templ);
  }
  return templ;
}

}  // namespace gin
