// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

int RunHelper(base::TestSuite* test_suite) {
  base::SingleThreadTaskExecutor task_executor;
  return test_suite->Run();
}

}  // namespace

// Located in third_party/angle/src/tests/test_utils/ANGLETest.cpp.
// Defined here so we can avoid depending on the ANGLE headers.
void ANGLEProcessTestArgs(int *argc, char *argv[]);
void RegisterContextCompatibilityTests();

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  ANGLEProcessTestArgs(&argc, argv);
  testing::InitGoogleMock(&argc, argv);
  RegisterContextCompatibilityTests();
  base::TestSuite test_suite(argc, argv);
  int rt = base::LaunchUnitTestsWithOptions(
      argc, argv,
      1,     // Run tests serially.
      0,     // Disable batching.
      true,  // Use job objects.
      base::BindOnce(&RunHelper, base::Unretained(&test_suite)));
  return rt;
}
