// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/message_loop/message_loop.h"
#include "base/test/test_suite.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_launcher.h"
#include "fuchsia/engine/common.h"
#include "fuchsia/engine/test/webrunner_browser_test.h"
#include "fuchsia/engine/webrunner_main_delegate.h"
#include "fuchsia/fidl/chromium/web/cpp/fidl.h"
#include "ui/ozone/public/ozone_switches.h"

namespace cr_fuchsia {
namespace test {

namespace {

class WebRunnerTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  WebRunnerTestLauncherDelegate() = default;
  ~WebRunnerTestLauncherDelegate() override = default;

  // content::TestLauncherDelegate implementation:
  int RunTestSuite(int argc, char** argv) override {
    base::TestSuite test_suite(argc, argv);
    // Browser tests are expected not to tear-down various globals.
    test_suite.DisableCheckForLeakedGlobals();
    return test_suite.Run();
  }

  bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) override {
    return true;
  }

  content::ContentMainDelegate* CreateContentMainDelegate() override {
    // Set up the channels for the Context service, but postpone client
    // binding until after the browser TaskRunners are up and running.
    zx::channel server_channel;
    zx::channel client_channel;
    zx_status_t result =
        zx::channel::create(0, &client_channel, &server_channel);
    ZX_CHECK(result == ZX_OK, result) << "zx::channel::create";
    WebRunnerBrowserTest::SetContextClientChannel(std::move(client_channel));

    return new WebRunnerMainDelegate(std::move(server_channel));
  }

 private:
  chromium::web::ContextPtr context_;

  DISALLOW_COPY_AND_ASSIGN(WebRunnerTestLauncherDelegate);
};

}  // namespace

}  // namespace test
}  // namespace cr_fuchsia

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kOzonePlatform, "headless");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableGpu);

  size_t parallel_jobs = base::NumParallelJobs();
  if (parallel_jobs > 1U) {
    parallel_jobs /= 2U;
  }
  cr_fuchsia::test::WebRunnerTestLauncherDelegate launcher_delegate;
  return LaunchTests(&launcher_delegate, parallel_jobs, argc, argv);
}
