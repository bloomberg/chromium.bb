// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "gin/modules/console.h"
#include "gin/modules/module_registry.h"
#include "gin/modules/timer.h"
#include "gin/test/file_runner.h"
#include "gin/test/gtest.h"
#include "mojo/apps/js/bindings/monotonic_clock.h"
#include "mojo/apps/js/bindings/threading.h"
#include "mojo/bindings/js/core.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace js {
namespace {

class TestRunnerDelegate : public gin::FileRunnerDelegate {
 public:
  TestRunnerDelegate() {
    AddBuiltinModule(gin::Console::kModuleName, gin::Console::GetModule);
    AddBuiltinModule(Core::kModuleName, Core::GetModule);
    AddBuiltinModule(gin::TimerModule::kName, gin::TimerModule::GetModule);
    AddBuiltinModule(apps::MonotonicClock::kModuleName,
                     apps::MonotonicClock::GetModule);
    AddBuiltinModule(apps::Threading::kModuleName, apps::Threading::GetModule);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRunnerDelegate);
};

void RunTest(std::string test, bool run_until_idle) {
  base::FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("mojo")
             .AppendASCII("apps")
             .AppendASCII("js")
             .AppendASCII("bindings")
             .AppendASCII(test);
  TestRunnerDelegate delegate;
  gin::RunTestFromFile(path, &delegate, run_until_idle);
}

// TODO(abarth): Should we autogenerate these stubs from GYP?

// http://crbug.com/351214
#if defined(OS_POSIX)
#define MAYBE_sample_test DISABLED_sample_test
#else
#define MAYBE_sample_test sample_test
#endif
TEST(JSTest, MAYBE_sample_test) {
  RunTest("sample_service_unittests.js", true);
}

// http://crbug.com/351214
#if defined(OS_POSIX)
#define MAYBE_connection DISABLED_connection
#else
#define MAYBE_connection connection
#endif
TEST(JSTest, MAYBE_connection) {
  RunTest("connection_unittests.js", false);
}

TEST(JSTest, monotonic_clock) {
  RunTest("monotonic_clock_unittests.js", false);
}

}  // namespace
}  // namespace js
}  // namespace mojo
