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

using testing::Contains;

namespace headless {

class VirtualTimeBrowserTest : public HeadlessAsyncDevTooledBrowserTest,
                               public emulation::ExperimentalObserver,
                               public page::ExperimentalObserver,
                               public runtime::Observer {
 public:
  explicit VirtualTimeBrowserTest(const std::string& initial_url)
      : initial_url_(initial_url) {}

  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    devtools_client_->GetEmulation()->GetExperimental()->AddObserver(this);
    devtools_client_->GetPage()->GetExperimental()->AddObserver(this);
    devtools_client_->GetRuntime()->AddObserver(this);

    devtools_client_->GetPage()->Enable(base::Bind(
        &VirtualTimeBrowserTest::PageEnabled, base::Unretained(this)));
    devtools_client_->GetRuntime()->Enable(base::Bind(
        &VirtualTimeBrowserTest::RuntimeEnabled, base::Unretained(this)));
  }

  void PageEnabled() {
    page_enabled = true;
    MaybeSetVirtualTimePolicy();
  }

  void RuntimeEnabled() {
    runtime_enabled = true;
    MaybeSetVirtualTimePolicy();
  }

  virtual void MaybeSetVirtualTimePolicy() = 0;

  void SetVirtualTimePolicyDone(
      std::unique_ptr<emulation::SetVirtualTimePolicyResult>) {
    // Virtual time is paused, so start navigating.
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL(initial_url_).spec());
  }

  void OnFrameStartedLoading(
      const page::FrameStartedLoadingParams& params) override {
    if (initial_load_seen_)
      return;
    initial_load_seen_ = true;
    // The navigation is underway, so allow virtual time to advance while
    // network fetches are not pending.
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(
                emulation::VirtualTimePolicy::PAUSE_IF_NETWORK_FETCHES_PENDING)
            .SetBudget(5000)
            .Build());
  }

  // runtime::Observer implementation:
  void OnConsoleAPICalled(
      const runtime::ConsoleAPICalledParams& params) override {
    // We expect the arguments always to be a single string.
    const std::vector<std::unique_ptr<runtime::RemoteObject>>& args =
        *params.GetArgs();
    if (args.size() == 1u && args[0]->HasValue())
      log_.push_back(args[0]->GetValue()->GetString());
  }

  const std::string initial_url_;
  std::vector<std::string> log_;
  bool initial_load_seen_ = false;
  bool page_enabled = false;
  bool runtime_enabled = false;
};

class VirtualTimeObserverTest : public VirtualTimeBrowserTest {
 public:
  VirtualTimeObserverTest()
      : VirtualTimeBrowserTest("/virtual_time_test.html") {}

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    std::vector<std::string> expected_log = {"step1",
                                             "Advanced to 100ms",
                                             "step2",
                                             "Paused @ 100ms",
                                             "Advanced to 200ms",
                                             "step3",
                                             "Paused @ 200ms",
                                             "Advanced to 300ms",
                                             "step4",
                                             "pass"};
    // Note after the PASS step there are a number of virtual time advances, but
    // this list seems to be non-deterministic because there's all sorts of
    // timers in the system.  We don't really care about that here.
    ASSERT_GE(log_.size(), expected_log.size());
    for (size_t i = 0; i < expected_log.size(); i++) {
      EXPECT_EQ(expected_log[i], log_[i]) << "At index " << i;
    }
    EXPECT_THAT(log_, Contains("Advanced to 5000ms"));
    EXPECT_THAT(log_, Contains("Paused @ 5000ms"));
    FinishAsynchronousTest();
  }

  void MaybeSetVirtualTimePolicy() override {
    if (!page_enabled || !runtime_enabled)
      return;

    // To avoid race conditions start with virtual time paused.
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(emulation::VirtualTimePolicy::PAUSE)
            .Build(),
        base::Bind(&VirtualTimeBrowserTest::SetVirtualTimePolicyDone,
                   base::Unretained(this)));
  }

  void OnVirtualTimeAdvanced(
      const emulation::VirtualTimeAdvancedParams& params) override {
    log_.push_back(
        base::StringPrintf("Advanced to %dms", params.GetVirtualTimeElapsed()));
  }

  void OnVirtualTimePaused(
      const emulation::VirtualTimePausedParams& params) override {
    log_.push_back(
        base::StringPrintf("Paused @ %dms", params.GetVirtualTimeElapsed()));
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(VirtualTimeObserverTest);

class MaxVirtualTimeTaskStarvationCountTest : public VirtualTimeBrowserTest {
 public:
  MaxVirtualTimeTaskStarvationCountTest()
      : VirtualTimeBrowserTest("/virtual_time_starvation_test.html") {}

  void MaybeSetVirtualTimePolicy() override {
    if (!page_enabled || !runtime_enabled)
      return;

    // To avoid race conditions start with virtual time paused.
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(emulation::VirtualTimePolicy::PAUSE)
            .SetMaxVirtualTimeTaskStarvationCount(100)
            .Build(),
        base::Bind(&VirtualTimeBrowserTest::SetVirtualTimePolicyDone,
                   base::Unretained(this)));
  }

  // emulation::Observer implementation:
  void OnVirtualTimeBudgetExpired(
      const emulation::VirtualTimeBudgetExpiredParams& params) override {
    // If SetMaxVirtualTimeTaskStarvationCount was not set, this callback would
    // never fire.
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(MaxVirtualTimeTaskStarvationCountTest);

}  // namespace headless
