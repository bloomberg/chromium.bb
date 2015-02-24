// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/sys_info.h"
#include "chromecast/app/cast_main_delegate.h"
#include "chromecast/common/chromecast_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace shell {

namespace {

// Duplicate switch to avoid dependency on chromecast/internal.
const char kSwitchesAVSettingsUnixSocketPath[] = "av-settings-unix-socket-path";

const char kAvSettingsUnixSocketPath[] = "/tmp/avsettings";
const char kTestTypeBrowser[] = "browser";

class BrowserTestSuite : public content::ContentTestSuiteBase {
 public:
  BrowserTestSuite(int argc, char** argv)
      : content::ContentTestSuiteBase(argc, argv) {
  }
  ~BrowserTestSuite() override {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserTestSuite);
};

class ChromecastTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  ChromecastTestLauncherDelegate() {}
  ~ChromecastTestLauncherDelegate() override {}

  int RunTestSuite(int argc, char** argv) override {
    return BrowserTestSuite(argc, argv).Run();
  }

  bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) override {
    // TODO(gunsch): handle temp_data_dir
    command_line->AppendSwitch(switches::kNoWifi);
    command_line->AppendSwitchASCII(switches::kTestType, kTestTypeBrowser);
    command_line->AppendSwitchASCII(kSwitchesAVSettingsUnixSocketPath,
                                    kAvSettingsUnixSocketPath);
    return true;
  }

 protected:
  content::ContentMainDelegate* CreateContentMainDelegate() override {
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
