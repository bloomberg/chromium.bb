// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "gin/modules/module_registry.h"
#include "gin/test/file_runner.h"
#include "gin/test/gtest.h"
#include "mojo/public/bindings/js/runner_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace js {
namespace {

class TestRunnerDelegate : public RunnerDelegate {
  virtual void DidCreateContext(gin::Runner* runner) MOJO_OVERRIDE {
    RunnerDelegate::DidCreateContext(runner);

    v8::Handle<v8::Context> context = runner->context();
    gin::ModuleRegistry* registry =
        gin::ModuleRegistry::From(context);

    registry->AddBuiltinModule(runner->isolate(), "gtest",
                               gin::GetGTestTemplate(runner->isolate()));
  }
};

void RunTest(std::string test) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("mojo")
             .AppendASCII("public")
             .AppendASCII("bindings")
             .AppendASCII("js")
             .AppendASCII(test);
  TestRunnerDelegate delegate;
  gin::RunTestFromFile(path, &delegate);
}

// TODO(abarth): Should we autogenerate these stubs from GYP?
TEST(JSTest, core) {
  RunTest("core_unittests.js");
}

}  // namespace
}  // namespace js
}  // namespace mojo
