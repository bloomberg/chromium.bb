// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/test_suite.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/setup/setup_util_unittest.h"

int main(int argc, char** argv) {
  TestSuite test_suite(argc, argv);

  // Handle the --adjust-process-priority switch, which is used to test the
  // installer::AdjustProcessPriority() function in a subprocess.
  if (CommandLine::ForCurrentProcess()->HasSwitch(kAdjustProcessPriority))
    return DoProcessPriorityAdjustment();

  // Register Chrome Path provider so that we can get test data dir.
  chrome::RegisterPathProvider();

  return test_suite.Run();
}
