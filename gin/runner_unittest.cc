// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/runner.h"

#include "base/compiler_specific.h"
#include "gin/converter.h"
#include "gin/public/isolate_holder.h"
#include "testing/gtest/include/gtest/gtest.h"

using v8::Isolate;
using v8::Object;
using v8::Script;
using v8::String;

namespace gin {

TEST(RunnerTest, Run) {
  std::string source = "this.result = 'PASS';\n";

  gin::IsolateHolder instance;

  RunnerDelegate delegate;
  Isolate* isolate = instance.isolate();
  Runner runner(&delegate, isolate);
  Runner::Scope scope(&runner);
  runner.Run(source, "test_data.js");

  std::string result;
  EXPECT_TRUE(Converter<std::string>::FromV8(isolate,
      runner.global()->Get(StringToV8(isolate, "result")),
      &result));
  EXPECT_EQ("PASS", result);
}

}  // namespace gin
