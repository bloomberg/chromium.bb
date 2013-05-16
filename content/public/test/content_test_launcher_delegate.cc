// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_test_launcher_delegate.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/shell/app/shell_main_delegate.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_suite.h"

namespace content {

ContentTestLauncherDelegate::ContentTestLauncherDelegate() {
}

ContentTestLauncherDelegate::~ContentTestLauncherDelegate() {
}

std::string ContentTestLauncherDelegate::GetEmptyTestName() {
  return std::string();
}

int ContentTestLauncherDelegate::RunTestSuite(int argc, char** argv) {
  return ContentBrowserTestSuite(argc, argv).Run();
}

bool ContentTestLauncherDelegate::AdjustChildProcessCommandLine(
    CommandLine* command_line, const base::FilePath& temp_data_dir) {
  command_line->AppendSwitchPath(switches::kContentShellDataPath,
                                 temp_data_dir);
  command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  return true;
}

ContentMainDelegate* ContentTestLauncherDelegate::CreateContentMainDelegate() {
  return new ShellMainDelegate();
}

}  // namespace
