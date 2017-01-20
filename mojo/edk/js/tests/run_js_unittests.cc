// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "gin/modules/console.h"
#include "gin/modules/module_registry.h"
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
    AddBuiltinModule(Threading::kModuleName, Threading::GetModule);
    AddBuiltinModule(Support::kModuleName, Support::GetModule);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRunnerDelegate);
};

void RunTest(std::string test, bool run_until_idle) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("mojo")
             .AppendASCII("public")
             .AppendASCII("js")
             .AppendASCII("tests")
             .AppendASCII(test);
  TestRunnerDelegate delegate;
  gin::RunTestFromFile(path, &delegate, run_until_idle);
}

// TODO(abarth): Should we autogenerate these stubs from GYP?
TEST(JSTest, Binding) {
  RunTest("binding_unittest.js", false);
}

TEST(JSTest, Codec) {
  RunTest("codec_unittest.js", true);
}

TEST(JSTest, Connection) {
  RunTest("connection_unittest.js", false);
}

TEST(JSTest, Core) {
  RunTest("core_unittest.js", true);
}

TEST(JSTest, InterfacePtr) {
  RunTest("interface_ptr_unittest.js", false);
}

TEST(JSTest, SampleService) {
  RunTest("sample_service_unittest.js", false);
}

TEST(JSTest, Struct) {
  RunTest("struct_unittest.js", true);
}

TEST(JSTest, Union) {
  RunTest("union_unittest.js", true);
}

TEST(JSTest, Validation) {
  RunTest("validation_unittest.js", true);
}

}  // namespace
}  // namespace js
}  // namespace edk
}  // namespace mojo
