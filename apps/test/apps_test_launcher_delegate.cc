// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/test/apps_test_launcher_delegate.h"

#include "apps/shell/app/shell_main_delegate.h"
#include "base/test/test_suite.h"

namespace apps {

int AppsTestLauncherDelegate::RunTestSuite(int argc, char** argv) {
  return base::TestSuite(argc, argv).Run();
}

bool AppsTestLauncherDelegate::AdjustChildProcessCommandLine(
    base::CommandLine* command_line,
    const base::FilePath& temp_data_dir) {
  return true;
}

content::ContentMainDelegate*
AppsTestLauncherDelegate::CreateContentMainDelegate() {
  return new ShellMainDelegate();
}

}  // namespace apps
