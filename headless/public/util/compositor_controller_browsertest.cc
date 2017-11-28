// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/compositor_controller.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/util/virtual_time_controller.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

// BeginFrameControl is not supported on Mac.
#if !defined(OS_MACOSX)

namespace {
static constexpr base::TimeDelta kAnimationFrameInterval =
    base::TimeDelta::FromMilliseconds(16);
static constexpr base::TimeDelta kWaitForCompositorReadyFrameDelay =
    base::TimeDelta::FromMilliseconds(20);
}  // namespace

class CompositorControllerBrowserTest
    : public HeadlessAsyncDevTooledBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessAsyncDevTooledBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(cc::switches::kRunAllCompositorStagesBeforeDraw);
    command_line->AppendSwitch(switches::kDisableNewContentRenderingTimeout);
  }

  bool GetEnableBeginFrameControl() override { return true; }

  void RunDevTooledTest() override {
    virtual_time_controller_ =
        base::MakeUnique<VirtualTimeController>(devtools_client_.get());
    compositor_controller_ = base::MakeUnique<CompositorController>(
        browser()->BrowserMainThread(), devtools_client_.get(),
        virtual_time_controller_.get(), kAnimationFrameInterval,
        kWaitForCompositorReadyFrameDelay);

    compositor_controller_->WaitForCompositorReady(
        base::Bind(&CompositorControllerBrowserTest::OnCompositorReady,
                   base::Unretained(this)));
  }

  void OnCompositorReady() {
    // Request animation frames in the main frame.
    devtools_client_->GetRuntime()->Evaluate(
        "window.rafCount = 0;"
        "function onRaf(timestamp) {"
        "  window.rafCount++;"
        "  window.requestAnimationFrame(onRaf);"
        "};"
        "window.requestAnimationFrame(onRaf);",
        base::Bind(&CompositorControllerBrowserTest::OnRafReady,
                   base::Unretained(this)));
  }

  void OnRafReady(std::unique_ptr<runtime::EvaluateResult> result) {
    EXPECT_NE(nullptr, result);
    EXPECT_FALSE(result->HasExceptionDetails());

    virtual_time_controller_->GrantVirtualTimeBudget(
        emulation::VirtualTimePolicy::ADVANCE,
        kNumFrames * kAnimationFrameInterval, base::Bind([]() {}),
        base::Bind(&CompositorControllerBrowserTest::OnVirtualTimeBudgetExpired,
                   base::Unretained(this)));
  }

  void OnVirtualTimeBudgetExpired() {
    // Get animation frame count.
    devtools_client_->GetRuntime()->Evaluate(
        "window.rafCount",
        base::Bind(&CompositorControllerBrowserTest::OnGetRafCount,
                   base::Unretained(this)));
  }

  void OnGetRafCount(std::unique_ptr<runtime::EvaluateResult> result) {
    EXPECT_NE(nullptr, result);
    EXPECT_FALSE(result->HasExceptionDetails());

    EXPECT_EQ(kNumFrames, result->GetResult()->GetValue()->GetInt());
    FinishAsynchronousTest();
  }

 private:
  static constexpr int kNumFrames = 3;

  std::unique_ptr<VirtualTimeController> virtual_time_controller_;
  std::unique_ptr<CompositorController> compositor_controller_;
};

/* static */
constexpr int CompositorControllerBrowserTest::kNumFrames;

HEADLESS_ASYNC_DEVTOOLED_TEST_F(CompositorControllerBrowserTest);

#endif  // !defined(OS_MACOSX)

}  // namespace headless
