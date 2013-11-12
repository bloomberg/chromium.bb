// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "gin/converter.h"
#include "gin/runner.h"
#include "gin/test/gtest.h"
#include "mojo/public/bindings/js/runner_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using v8::Isolate;
using v8::Object;
using v8::Script;
using v8::Value;

namespace mojo {
namespace js {
namespace {

class TestRunnerDelegate : public RunnerDelegate {
 public:
  virtual ~TestRunnerDelegate() {}

  virtual v8::Handle<Object> CreateRootObject(
      gin::Runner* runner) MOJO_OVERRIDE {
    v8::Handle<Object> root = RunnerDelegate::CreateRootObject(runner);
    root->Set(gin::StringToSymbol(runner->isolate(), "gtest"),
              gin::GetGTestTemplate(runner->isolate())->NewInstance());
    return root;
  }
};

std::string GetExceptionInfo(const v8::TryCatch& try_catch) {
  std::string info;
  gin::ConvertFromV8(try_catch.Message()->Get(), &info);
  return info;
}

void RunTestFromFile(const base::FilePath& path) {
  EXPECT_TRUE(base::PathExists(path)) << path.LossyDisplayName();
  std::string source;
  EXPECT_TRUE(ReadFileToString(path, &source));
  Isolate* isolate = Isolate::GetCurrent();

  TestRunnerDelegate delegate;
  gin::Runner runner(&delegate, isolate);
  gin::Runner::Scope scope(&runner);

  v8::TryCatch try_catch;
  runner.Run(Script::New(gin::StringToV8(isolate, source)));

  EXPECT_FALSE(try_catch.HasCaught()) << GetExceptionInfo(try_catch);
}

void RunTest(std::string test) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("mojo")
             .AppendASCII("public")
             .AppendASCII("bindings")
             .AppendASCII("js")
             .AppendASCII(test);
  RunTestFromFile(path);
}

// TODO(abarth): Should we autogenerate these stubs from GYP?
TEST(Harness, mojo_unittests_js) {
  RunTest("mojo_unittests.js");
}

TEST(Harness, core_unittests_js) {
  RunTest("core_unittests.js");
}

}  // namespace
}  // namespace js
}  // namespace mojo
