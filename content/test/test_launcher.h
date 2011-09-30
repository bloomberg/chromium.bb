// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_LAUNCHER_H_
#define CONTENT_TEST_TEST_LAUNCHER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"

class CommandLine;

namespace test_launcher {

extern const char kGTestFilterFlag[];
extern const char kGTestHelpFlag[];
extern const char kGTestListTestsFlag[];
extern const char kGTestRepeatFlag[];
extern const char kGTestRunDisabledTestsFlag[];
extern const char kGTestOutputFlag[];

extern const char kSingleProcessTestsFlag[];
extern const char kSingleProcessTestsAndChromeFlag[];

extern const char kHelpFlag[];

class TestLauncherDelegate {
 public:
  virtual void EarlyInitialize() = 0;
  virtual bool Run(int argc, char** argv, int* return_code) = 0;
  virtual bool AdjustChildProcessCommandLine(CommandLine* command_line) = 0;

 protected:
  virtual ~TestLauncherDelegate();
};

int LaunchTests(TestLauncherDelegate* launcher_delegate,
                int argc,
                char** argv) WARN_UNUSED_RESULT;

}  // namespace test_launcher

#endif  // CONTENT_TEST_TEST_LAUNCHER_H_