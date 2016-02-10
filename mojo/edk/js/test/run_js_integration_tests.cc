// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "gin/modules/console.h"
#include "gin/modules/module_registry.h"
#include "gin/modules/timer.h"
#include "gin/test/file_runner.h"
#include "gin/test/gtest.h"
#include "mojo/edk/js/core.h"
#include "mojo/edk/js/support.h"
#include "mojo/edk/js/threading.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace edk {
namespace js {
namespace {

class TestRunnerDelegate : public gin::FileRunnerDelegate {
 public:
  TestRunnerDelegate() {
    AddBuiltinModule(gin::Console::kModuleName, gin::Console::GetModule);
    AddBuiltinModule(Core::kModuleName, Core::GetModule);
    AddBuiltinModule(gin::TimerModule::kName, gin::TimerModule::GetModule);
    AddBuiltinModule(Threading::kModuleName, Threading::GetModule);
    AddBuiltinModule(Support::kModuleName, Support::GetModule);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(TestRunnerDelegate);
};

void RunTest(std::string test) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("mojo")
             .AppendASCII("edk")
             .AppendASCII("js")
             .AppendASCII("tests")
             .AppendASCII(test);
  TestRunnerDelegate delegate;
  gin::RunTestFromFile(path, &delegate, false);
}

TEST(JSTest, connection) {
  RunTest("connection_tests.js");
}

TEST(JSTest, sample_service) {
  RunTest("sample_service_tests.js");
}

}  // namespace
}  // namespace js
}  // namespace edk
}  // namespace mojo
