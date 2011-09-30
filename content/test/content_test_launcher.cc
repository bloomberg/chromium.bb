// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_launcher.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_temp_dir.h"
#include "content/test/content_test_suite.h"

class ContentTestLauncherDelegate : public test_launcher::TestLauncherDelegate {
 public:
  ContentTestLauncherDelegate() {
  }

  virtual ~ContentTestLauncherDelegate() {
  }

  virtual void EarlyInitialize() OVERRIDE {
  }

  virtual bool Run(int argc, char** argv, int* return_code) OVERRIDE {
    CommandLine* command_line = CommandLine::ForCurrentProcess();

    // TODO(pkasting): This "single_process vs. single-process" design is
    // terrible UI.  Instead, there should be some sort of signal flag on the
    // command line, with all subsequent arguments passed through to the
    // underlying browser.
    if (command_line->HasSwitch(test_launcher::kSingleProcessTestsFlag) ||
        command_line->HasSwitch(
            test_launcher::kSingleProcessTestsAndChromeFlag) ||
        command_line->HasSwitch(test_launcher::kGTestListTestsFlag) ||
        command_line->HasSwitch(test_launcher::kGTestHelpFlag)) {
      *return_code = ContentTestSuite(argc, argv).Run();
      return true;
    }

    return false;
  }

  virtual bool AdjustChildProcessCommandLine(
      CommandLine* command_line) OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentTestLauncherDelegate);
};

int main(int argc, char** argv) {
  ContentTestLauncherDelegate launcher_delegate;
  return test_launcher::LaunchTests(&launcher_delegate, argc, argv);
}
