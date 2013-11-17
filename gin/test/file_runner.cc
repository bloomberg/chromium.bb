// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/test/file_runner.h"

#include "base/file_util.h"
#include "gin/converter.h"
#include "gin/modules/module_registry.h"
#include "gin/test/gtest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {

namespace {

std::string GetExceptionInfo(const v8::TryCatch& try_catch) {
  std::string info;
  ConvertFromV8(try_catch.Message()->Get(), &info);
  return info;
}

}  // namespace

FileRunnerDelegate::FileRunnerDelegate() {
}

FileRunnerDelegate::~FileRunnerDelegate() {
}

v8::Handle<v8::ObjectTemplate> FileRunnerDelegate::GetGlobalTemplate(
    Runner* runner) {
  v8::Handle<v8::ObjectTemplate> templ = v8::ObjectTemplate::New();
  ModuleRegistry::RegisterGlobals(runner->isolate(), templ);
  return templ;
}

void FileRunnerDelegate::DidCreateContext(Runner* runner) {
  v8::Handle<v8::Context> context = runner->context();
  ModuleRegistry* registry = ModuleRegistry::From(context);

  registry->AddBuiltinModule(runner->isolate(), "gtest",
                             GetGTestTemplate(runner->isolate()));
}

void RunTestFromFile(const base::FilePath& path, RunnerDelegate* delegate) {
  ASSERT_TRUE(base::PathExists(path)) << path.LossyDisplayName();
  std::string source;
  ASSERT_TRUE(ReadFileToString(path, &source));
  gin::Runner runner(delegate, v8::Isolate::GetCurrent());
  gin::Runner::Scope scope(&runner);

  v8::TryCatch try_catch;
  runner.Run(source);
  EXPECT_FALSE(try_catch.HasCaught()) << GetExceptionInfo(try_catch);

  v8::Handle<v8::Value> result = runner.context()->Global()->Get(
      StringToSymbol(runner.isolate(), "result"));
  std::string result_string;
  ASSERT_TRUE(ConvertFromV8(result, &result_string));
  EXPECT_EQ("PASS", result_string);
}

}  // namespace gin
