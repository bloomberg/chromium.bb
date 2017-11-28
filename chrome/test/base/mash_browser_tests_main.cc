// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/test_launcher.h"
#include "ui/aura/env.h"
#include "ui/base/ui_base_switches.h"

namespace {

// Enumeration of the possible chrome-ash configurations.
enum class AshConfig {
  // Aura is backed by mus, but chrome and ash are still in the same process.
  MUS,

  // Aura is backed by mus and chrome and ash are in separate processes. In
  // this mode chrome code can only use ash code in ash/public/cpp.
  MASH,
};

class MusTestLauncherDelegate : public ChromeTestLauncherDelegate {
 public:
  explicit MusTestLauncherDelegate(ChromeTestSuiteRunner* runner)
      : ChromeTestLauncherDelegate(runner),
        config_(
            base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kMash)
                ? AshConfig::MASH
                : AshConfig::MUS) {}

  ~MusTestLauncherDelegate() override {}

 private:
  // ChromeTestLauncherDelegate:
  int RunTestSuite(int argc, char** argv) override {
    content::GetContentMainParams()->env_mode = aura::Env::Mode::MUS;
    content::GetContentMainParams()->create_discardable_memory =
        (config_ == AshConfig::MUS);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kMus, switches::kMusHostVizValue);
    return ChromeTestLauncherDelegate::RunTestSuite(argc, argv);
  }

  AshConfig config_;

  DISALLOW_COPY_AND_ASSIGN(MusTestLauncherDelegate);
};

}  // namespace

bool RunMashBrowserTests(int argc, char** argv, int* exit_code) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kMash) &&
      !command_line->HasSwitch(switches::kMus)) {
    // Currently launching content_package_services via the browser_tests binary
    // will lead to a nested test suite, trying to run all tests again. However
    // they will be in a strange mixed mode, of a local-Ash, but non-local Aura.
    //
    // This leads to continuous crashes in OzonePlatform.
    //
    // For now disable this launch until the requesting site can be identified.
    //
    // TODO(jonross): find an appropriate way to launch content_package_services
    // within the mash_browser_tests (crbug.com/738449)
    if (command_line->GetSwitchValueASCII("service-name") ==
        content::mojom::kPackagedServicesServiceName) {
      return true;
    }
    return false;
  }

  size_t parallel_jobs = base::NumParallelJobs();
  ChromeTestSuiteRunner chrome_test_suite_runner;
  MusTestLauncherDelegate test_launcher_delegate(&chrome_test_suite_runner);
  if (command_line->HasSwitch(switches::kMash) && parallel_jobs > 1U)
    parallel_jobs /= 2U;
  *exit_code =
      LaunchChromeTests(parallel_jobs, &test_launcher_delegate, argc, argv);
  return true;
}
