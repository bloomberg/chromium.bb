// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;

namespace headless {

class VirtualTimeBrowserTest : public HeadlessAsyncDevTooledBrowserTest,
                               public emulation::ExperimentalObserver,
                               public runtime::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    devtools_client_->GetEmulation()->GetExperimental()->AddObserver(this);
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(
                emulation::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING)
            .SetBudget(5000)
            .Build(),
        base::Bind(&VirtualTimeBrowserTest::SetVirtualTimePolicyDone,
                   base::Unretained(this)));
    devtools_client_->GetRuntime()->AddObserver(this);
    devtools_client_->GetRuntime()->Enable();
  }

  void SetVirtualTimePolicyDone(
      std::unique_ptr<headless::emulation::SetVirtualTimePolicyResult>) {
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/virtual_time_test.html").spec());
  }

  // runtime::Observer implementation:
  void OnConsoleAPICalled(
      const runtime::ConsoleAPICalledParams& params) override {
    // We expect the arguments always to be a single string.
    const std::vector<std::unique_ptr<runtime::RemoteObject>>& args =
        *params.GetArgs();
    std::string message;
    if (args.size() == 1u && args[0]->HasValue() &&
        args[0]->GetValue()->GetAsString(&message)) {
      log_.push_back(message);
    }
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    EXPECT_THAT(log_,
                ElementsAre("Paused @ 0ms", "step1", "step2", "Paused @ 100ms",
                            "step3", "Paused @ 200ms", "step4", "pass"));
    FinishAsynchronousTest();
  }

  void OnVirtualTimePaused(
      const emulation::VirtualTimePausedParams& params) override {
    log_.push_back(
        base::StringPrintf("Paused @ %dms", params.GetVirtualTimeElapsed()));
  }

  std::vector<std::string> log_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(VirtualTimeBrowserTest);

}  // namespace headless
