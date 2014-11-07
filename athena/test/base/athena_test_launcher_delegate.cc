// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/base/athena_test_launcher_delegate.h"

#include "athena/main/athena_main_delegate.h"
#include "base/test/test_suite.h"

namespace athena {
namespace test {

int AthenaTestLauncherDelegate::RunTestSuite(int argc, char** argv) {
  return base::TestSuite(argc, argv).Run();
}

bool AthenaTestLauncherDelegate::AdjustChildProcessCommandLine(
    base::CommandLine* command_line,
    const base::FilePath& temp_data_dir) {
  return true;
}

content::ContentMainDelegate*
AthenaTestLauncherDelegate::CreateContentMainDelegate() {
  return new AthenaMainDelegate();
}

}  // namespace test
}  // namespace extensions
