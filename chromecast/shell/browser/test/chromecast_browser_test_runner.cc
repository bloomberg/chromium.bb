// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/sys_info.h"
#include "chromecast/shell/app/cast_main_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace shell {

namespace {

const char kTestTypeBrowser[] = "browser";

class BrowserTestSuite : public content::ContentTestSuiteBase {
 public:
  BrowserTestSuite(int argc, char** argv)
      : content::ContentTestSuiteBase(argc, argv) {
  }
  virtual ~BrowserTestSuite() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserTestSuite);
};

class ChromecastTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  ChromecastTestLauncherDelegate() {}
  virtual ~ChromecastTestLauncherDelegate() {}

  virtual int RunTestSuite(int argc, char** argv) OVERRIDE {
    return BrowserTestSuite(argc, argv).Run();
  }

  virtual bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) OVERRIDE {
    // TODO(gunsch): handle temp_data_dir
    command_line->AppendSwitchASCII(switches::kTestType, kTestTypeBrowser);
    return true;
  }

 protected:
  virtual content::ContentMainDelegate* CreateContentMainDelegate() OVERRIDE {
    return new CastMainDelegate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromecastTestLauncherDelegate);
};

}  // namespace

}  // namespace shell
}  // namespace chromecast

int main(int argc, char** argv) {
  int default_jobs = std::max(1, base::SysInfo::NumberOfProcessors() / 2);
  chromecast::shell::ChromecastTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, default_jobs, argc, argv);
}
