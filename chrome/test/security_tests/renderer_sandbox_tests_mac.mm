// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/test/security_tests/renderer_sandbox_tests_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/file_util.h"

//--------------------- Sandbox Tests ---------------------
// Below is a list of test functions that check the renderer sandbox.
// In order for a test function to be executed, it must be added to the
// |sandbox_test_cases| array in +[RendererSandboxTestsRunner runTests] below.
// TODO(ofri): Consider moving these to another file once there are enough tests
// to justify.

// Test case for checking sandboxing of clipboard access.
bool SandboxTestClipboardTestCase(void) {
  return [NSPasteboard generalPasteboard] == nil;
}

// Test case for checking sandboxing of filesystem apis.
bool SandboxTestFileAccessTestCase(void) {
  int fdes = open("/etc/passwd", O_RDONLY);
  if (fdes == -1) {
    return true;
  } else {
    close(fdes);
    return false;
  }
}

//--------------------- Test Execution ---------------------

static LogRendererSandboxTestMessage log_function = NULL;

static inline void LogInfoMessage(std::string message) {
  log_function(message, false);
}

static inline void LogErrorMessage(std::string message) {
  log_function(message, true);
}

@implementation RendererSandboxTestsRunner

+ (void)setLogFunction:(LogRendererSandboxTestMessage)logFunction {
  log_function = logFunction;
}

+ (BOOL)runTests {
  // A test case entry. One must exist for each test.
  struct SandboxTestCase {
    std::string name;
    bool (*test_function)(void);
  };
  const struct SandboxTestCase sandbox_test_cases[] = {
#define DEFINE_TEST_CASE(testFunction) { #testFunction, testFunction }

    // The list of registered tests
    DEFINE_TEST_CASE(SandboxTestClipboardTestCase),
    DEFINE_TEST_CASE(SandboxTestFileAccessTestCase),

#undef DEFINE_TEST_CASE
    // Termination entry
    { "", NULL }
  };

  // Execute the tests
  BOOL tests_passed = YES;
  for (const struct SandboxTestCase* test_case = sandbox_test_cases;
       test_case->test_function != NULL;
       ++test_case) {
    LogInfoMessage("Running sandbox test: " + test_case->name);
    if (test_case->test_function()) {
      LogInfoMessage("Test: " + test_case->name + " - PASSED");
    } else {
      LogErrorMessage("Test: " + test_case->name + " - FAILED");
      tests_passed = NO;
    }
  }
  return tests_passed;
}

@end
