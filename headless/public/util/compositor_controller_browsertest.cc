// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/compositor_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/util/virtual_time_controller.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"

namespace headless {

// BeginFrameControl is not supported on Mac.
#if !defined(OS_MACOSX)

namespace {

static constexpr base::TimeDelta kAnimationFrameInterval =
    base::TimeDelta::FromMilliseconds(16);

class BeginFrameCounter : HeadlessDevToolsClient::RawProtocolListener {
 public:
  BeginFrameCounter(HeadlessDevToolsClient* client) : client_(client) {
    client_->SetRawProtocolListener(this);
  }

  ~BeginFrameCounter() override { client_->SetRawProtocolListener(nullptr); }

  bool OnProtocolMessage(const std::string& devtools_agent_host_id,
                         const std::string& json_message,
                         const base::DictionaryValue& parsed_message) override {
    const base::Value* id_value = parsed_message.FindKey("id");
    if (!id_value)
      return false;

    const base::DictionaryValue* result_dict;
    if (parsed_message.GetDictionary("result", &result_dict)) {
      bool has_damage;
      if (result_dict->GetBoolean("hasDamage", &has_damage))
        ++begin_frame_count_;
    }
    return false;
  }

  int begin_frame_count() const { return begin_frame_count_; }

 private:
  HeadlessDevToolsClient* client_;  // NOT OWNED.
  int begin_frame_count_ = 0;
};

bool DecodePNG(std::string png_data, SkBitmap* bitmap) {
  return gfx::PNGCodec::Decode(
      reinterpret_cast<unsigned const char*>(png_data.data()), png_data.size(),
      bitmap);
}

}  // namespace

class CompositorControllerBrowserTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  class AdditionalVirtualTimeBudget
      : public VirtualTimeController::RepeatingTask,
        public VirtualTimeController::Observer {
   public:
    AdditionalVirtualTimeBudget(VirtualTimeController* virtual_time_controller,
                                base::TimeDelta budget,
                                base::OnceClosure budget_expired_callback)
        : RepeatingTask(StartPolicy::START_IMMEDIATELY, 0),
          virtual_time_controller_(virtual_time_controller),
          budget_expired_callback_(std::move(budget_expired_callback)) {
      virtual_time_controller_->ScheduleRepeatingTask(this, budget);
      virtual_time_controller_->AddObserver(this);
      virtual_time_controller_->StartVirtualTime();
    }

    ~AdditionalVirtualTimeBudget() override {
      virtual_time_controller_->RemoveObserver(this);
      virtual_time_controller_->CancelRepeatingTask(this);
    }

    // headless::VirtualTimeController::RepeatingTask implementation:
    void IntervalElapsed(
        base::TimeDelta virtual_time,
        base::OnceCallback<void(ContinuePolicy)> continue_callback) override {
      std::move(continue_callback).Run(ContinuePolicy::NOT_REQUIRED);
    }

    // headless::VirtualTimeController::Observer:
    void VirtualTimeStarted(base::TimeDelta virtual_time_offset) override {}

    void VirtualTimeStopped(base::TimeDelta virtual_time_offset) override {
      std::move(budget_expired_callback_).Run();
      delete this;
    }

   private:
    headless::VirtualTimeController* const virtual_time_controller_;
    base::OnceClosure budget_expired_callback_;
  };

  void SetUp() override {
    EnablePixelOutput();
    if (GetParam()) {
      UseSoftwareCompositing();
      SetUpWithoutGPU();
    } else {
      HeadlessAsyncDevTooledBrowserTest::SetUp();
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessAsyncDevTooledBrowserTest::SetUpCommandLine(command_line);
    // See bit.ly/headless-rendering for why we use these flags.
    command_line->AppendSwitch(switches::kRunAllCompositorStagesBeforeDraw);
    command_line->AppendSwitch(switches::kDisableNewContentRenderingTimeout);
    command_line->AppendSwitch(cc::switches::kDisableCheckerImaging);
    command_line->AppendSwitch(cc::switches::kDisableThreadedAnimation);
    command_line->AppendSwitch(switches::kDisableThreadedScrolling);
    command_line->AppendSwitch(switches::kEnableSurfaceSynchronization);
  }

  bool GetEnableBeginFrameControl() override { return true; }

  void RunDevTooledTest() override {
    virtual_time_controller_ =
        std::make_unique<VirtualTimeController>(devtools_client_.get());
    const bool update_display_for_animations = false;
    compositor_controller_ = base::MakeUnique<CompositorController>(
        browser()->BrowserMainThread(), devtools_client_.get(),
        virtual_time_controller_.get(), kAnimationFrameInterval,
        update_display_for_animations);

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CompositorControllerBrowserTest::RunFirstBeginFrame,
                       base::Unretained(this)),
        base::TimeDelta::FromSeconds(1));
  }

  void RunFirstBeginFrame() {
    begin_frame_counter_ =
        base::MakeUnique<BeginFrameCounter>(devtools_client_.get());
    render_frame_submission_observer_ =
        std::make_unique<content::RenderFrameSubmissionObserver>(
            HeadlessWebContentsImpl::From(web_contents_)->web_contents());
    // AdditionalVirtualTimeBudget will self delete.
    new AdditionalVirtualTimeBudget(
        virtual_time_controller_.get(), kAnimationFrameInterval,
        base::BindOnce(
            &CompositorControllerBrowserTest::OnFirstBeginFrameComplete,
            base::Unretained(this)));
  }

  void OnFirstBeginFrameComplete() {
    // With surface sync enabled, we should have waited for the renderer's
    // CompositorFrame in the first BeginFrame.
    EXPECT_EQ(1, begin_frame_counter_->begin_frame_count());
    EXPECT_EQ(1, render_frame_submission_observer_->render_frame_count());

    // Request animation frames in the main frame. Each frame changes the body
    // background color.
    devtools_client_->GetRuntime()->Evaluate(
        "window.rafCount = 0;"
        "function onRaf(timestamp) {"
        "  window.rafCount++;"
        "  document.body.style.backgroundColor = '#' + window.rafCount * 100;"
        "  window.requestAnimationFrame(onRaf);"
        "};"
        "window.requestAnimationFrame(onRaf);",
        base::BindRepeating(&CompositorControllerBrowserTest::OnRafReady,
                            base::Unretained(this)));
  }

  void OnRafReady(std::unique_ptr<runtime::EvaluateResult> result) {
    EXPECT_NE(nullptr, result);
    EXPECT_FALSE(result->HasExceptionDetails());

    // AdditionalVirtualTimeBudget will self delete.
    new AdditionalVirtualTimeBudget(
        virtual_time_controller_.get(), kNumFrames * kAnimationFrameInterval,
        base::BindOnce(&CompositorControllerBrowserTest::OnRafBudgetExpired,
                       base::Unretained(this)));
  }

  void OnRafBudgetExpired() {
    EXPECT_EQ(1 + kNumFrames, begin_frame_counter_->begin_frame_count());
    // Even though the rAF made a change to the frame's background color, no
    // further CompositorFrames should have been produced for animations,
    // because update_display_for_animations is false.
    EXPECT_EQ(1, render_frame_submission_observer_->render_frame_count());

    // Get animation frame count.
    devtools_client_->GetRuntime()->Evaluate(
        "window.rafCount",
        base::BindRepeating(&CompositorControllerBrowserTest::OnGetRafCount,
                            base::Unretained(this)));
  }

  void OnGetRafCount(std::unique_ptr<runtime::EvaluateResult> result) {
    EXPECT_NE(nullptr, result);
    EXPECT_FALSE(result->HasExceptionDetails());

    EXPECT_EQ(kNumFrames, result->GetResult()->GetValue()->GetInt());

    compositor_controller_->CaptureScreenshot(
        headless_experimental::ScreenshotParamsFormat::PNG, 100,
        base::BindRepeating(&CompositorControllerBrowserTest::OnScreenshot,
                            base::Unretained(this)));
  }

  void OnScreenshot(const std::string& screenshot_data) {
    // Screenshot should have incurred a new CompositorFrame.
    EXPECT_EQ(2, render_frame_submission_observer_->render_frame_count());

    EXPECT_LT(0U, screenshot_data.length());

    if (screenshot_data.length()) {
      SkBitmap result_bitmap;
      EXPECT_TRUE(DecodePNG(screenshot_data, &result_bitmap));

      EXPECT_EQ(800, result_bitmap.width());
      EXPECT_EQ(600, result_bitmap.height());
      SkColor actual_color = result_bitmap.getColor(200, 200);
      // Screenshot was the fourth frame, so background color should be #400.
      SkColor expected_color = SkColorSetRGB(0x44, 0x00, 0x00);
      EXPECT_EQ(expected_color, actual_color);
    }

    render_frame_submission_observer_.reset();
    FinishAsynchronousTest();
  }

 protected:
  static constexpr int kNumFrames = 3;

  std::unique_ptr<VirtualTimeController> virtual_time_controller_;
  std::unique_ptr<CompositorController> compositor_controller_;

  std::unique_ptr<BeginFrameCounter> begin_frame_counter_;
  std::unique_ptr<content::RenderFrameSubmissionObserver>
      render_frame_submission_observer_;
};

/* static */
constexpr int CompositorControllerBrowserTest::kNumFrames;

HEADLESS_ASYNC_DEVTOOLED_TEST_P(CompositorControllerBrowserTest);

// Instantiate test case for both software and gpu compositing modes.
INSTANTIATE_TEST_CASE_P(CompositorControllerBrowserTests,
                        CompositorControllerBrowserTest,
                        ::testing::Bool());

#endif  // !defined(OS_MACOSX)

}  // namespace headless
