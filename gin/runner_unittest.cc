// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/runner.h"

#include "base/compiler_specific.h"
#include "gin/converter.h"
#include "testing/gtest/include/gtest/gtest.h"

using v8::Handle;
using v8::Isolate;
using v8::Object;
using v8::Script;
using v8::String;

namespace gin {
namespace {

class TestRunnerDelegate : public RunnerDelegate {
 public:
  virtual Handle<Object> CreateRootObject(Runner* runner) OVERRIDE {
    Isolate* isolate = runner->isolate();
    Handle<Object> root = Object::New();
    root->Set(StringToV8(isolate, "foo"), StringToV8(isolate, "bar"));
    return root;
  }
  virtual ~TestRunnerDelegate() {}
};

}

TEST(RunnerTest, Run) {
  std::string source =
      "function main(root) {\n"
      "  if (root.foo)\n"
      "    this.result = 'PASS';\n"
      "  else\n"
      "    this.result = 'FAIL';\n"
      "}\n";

  TestRunnerDelegate delegate;
  Isolate* isolate = Isolate::GetCurrent();
  Runner runner(&delegate, isolate);
  Runner::Scope scope(&runner);
  runner.Run(Script::New(StringToV8(isolate, source)));

  std::string result;
  EXPECT_TRUE(Converter<std::string>::FromV8(
      runner.global()->Get(StringToV8(isolate, "result")),
      &result));
  EXPECT_EQ("PASS", result);
}

}  // namespace gin
