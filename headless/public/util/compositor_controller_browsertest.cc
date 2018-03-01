// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/compositor_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/emulation.h"
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
    AdditionalVirtualTimeBudget(
        VirtualTimeController* virtual_time_controller,
        StartPolicy start_policy,
        base::TimeDelta budget,
        base::OnceClosure budget_expired_callback,
        base::OnceClosure virtual_time_started_callback = base::OnceClosure())
        : RepeatingTask(start_policy, 0),
          virtual_time_controller_(virtual_time_controller),
          budget_expired_callback_(std::move(budget_expired_callback)),
          virtual_time_started_callback_(
              std::move(virtual_time_started_callback)) {
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
    void VirtualTimeStarted(base::TimeDelta virtual_time_offset) override {
      if (virtual_time_started_callback_)
        std::move(virtual_time_started_callback_).Run();
    }

    void VirtualTimeStopped(base::TimeDelta virtual_time_offset) override {
      std::move(budget_expired_callback_).Run();
      delete this;
    }

   private:
    headless::VirtualTimeController* const virtual_time_controller_;
    base::OnceClosure budget_expired_callback_;
    base::OnceClosure virtual_time_started_callback_;
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

    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableSurfaceSynchronization);
  }

  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override {
    // Set an arbitrary initial time to enable base::Time/TimeTicks overrides.
    builder.SetInitialVirtualTime(base::Time::Now());
  }

  bool GetEnableBeginFrameControl() override { return true; }

  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    virtual_time_controller_ =
        std::make_unique<VirtualTimeController>(devtools_client_.get());
    const bool update_display_for_animations = false;
    compositor_controller_ = std::make_unique<CompositorController>(
        browser()->BrowserMainThread(), devtools_client_.get(),
        virtual_time_controller_.get(), GetAnimationFrameInterval(),
        update_display_for_animations);

    // Initially pause virtual time.
    devtools_client_->GetEmulation()->GetExperimental()->SetVirtualTimePolicy(
        emulation::SetVirtualTimePolicyParams::Builder()
            .SetPolicy(emulation::VirtualTimePolicy::PAUSE)
            .Build(),
        base::BindRepeating(
            &CompositorControllerBrowserTest::SetVirtualTimePolicyDone,
            base::Unretained(this)));
  }

 protected:
  virtual base::TimeDelta GetAnimationFrameInterval() const {
    return base::TimeDelta::FromMilliseconds(16);
  }

  virtual std::string GetTestFile() const { return "/blank.html"; }

  void SetVirtualTimePolicyDone(
      std::unique_ptr<emulation::SetVirtualTimePolicyResult>) {
    // Run a first BeginFrame to initialize surface. Wait a while before doing
    // so, since it takes a while before the compositor is ready for a
    // RenderFrameSubmissionObserver.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CompositorControllerBrowserTest::RunFirstBeginFrame,
                       base::Unretained(this)),
        base::TimeDelta::FromSeconds(1));
  }

  void RunFirstBeginFrame() {
    begin_frame_counter_ =
        std::make_unique<BeginFrameCounter>(devtools_client_.get());
    render_frame_submission_observer_ =
        std::make_unique<content::RenderFrameSubmissionObserver>(
            HeadlessWebContentsImpl::From(web_contents_)->web_contents());
    // AdditionalVirtualTimeBudget will self delete.
    new AdditionalVirtualTimeBudget(
        virtual_time_controller_.get(),
        AdditionalVirtualTimeBudget::StartPolicy::WAIT_FOR_NAVIGATION,
        GetAnimationFrameInterval(),
        base::BindOnce(
            &CompositorControllerBrowserTest::OnFirstBeginFrameComplete,
            base::Unretained(this)),
        base::BindOnce(&CompositorControllerBrowserTest::Navigate,
                       base::Unretained(this)));
  }

  void Navigate() {
    // Navigate (after the first BeginFrame) to start virtual time.
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL(GetTestFile()).spec());
  }

  virtual void OnFirstBeginFrameComplete() {
    // With surface sync enabled, we should have waited for the renderer's
    // CompositorFrame in the first BeginFrame.
    EXPECT_EQ(1, begin_frame_counter_->begin_frame_count());
    EXPECT_EQ(1, render_frame_submission_observer_->render_frame_count());
  }

  void CaptureDoubleScreenshot(
      const base::RepeatingCallback<void(const std::string&)>&
          screenshot_captured_callback) {
    compositor_controller_->CaptureScreenshot(
        headless_experimental::ScreenshotParamsFormat::PNG, 100,
        base::BindRepeating(
            &CompositorControllerBrowserTest::OnFirstScreenshotCaptured,
            base::Unretained(this), screenshot_captured_callback));
  }

  void OnFirstScreenshotCaptured(
      const base::RepeatingCallback<void(const std::string&)>&
          screenshot_captured_callback,
      const std::string& screenshot_data) {
    // Ignore result of first screenshot because of crbug.com/817364.
    // TODO(crbug.com/817364): Remove the second screenshot once bug is fixed.
    compositor_controller_->CaptureScreenshot(
        headless_experimental::ScreenshotParamsFormat::PNG, 100,
        screenshot_captured_callback);
  }

  void FinishCompositorControllerTest() {
    render_frame_submission_observer_.reset();
    FinishAsynchronousTest();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<VirtualTimeController> virtual_time_controller_;
  std::unique_ptr<CompositorController> compositor_controller_;

  std::unique_ptr<BeginFrameCounter> begin_frame_counter_;
  std::unique_ptr<content::RenderFrameSubmissionObserver>
      render_frame_submission_observer_;
};

// Runs requestAnimationFrame three times without updating display for
// animations and takes a screenshot.
class CompositorControllerRafBrowserTest
    : public CompositorControllerBrowserTest {
 private:
  void OnFirstBeginFrameComplete() override {
    CompositorControllerBrowserTest::OnFirstBeginFrameComplete();

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
        base::BindRepeating(&CompositorControllerRafBrowserTest::OnRafReady,
                            base::Unretained(this)));
  }

  void OnRafReady(std::unique_ptr<runtime::EvaluateResult> result) {
    EXPECT_NE(nullptr, result);
    EXPECT_FALSE(result->HasExceptionDetails());

    // AdditionalVirtualTimeBudget will self delete.
    new AdditionalVirtualTimeBudget(
        virtual_time_controller_.get(),
        AdditionalVirtualTimeBudget::StartPolicy::START_IMMEDIATELY,
        kNumFrames * GetAnimationFrameInterval(),
        base::BindOnce(&CompositorControllerRafBrowserTest::OnRafBudgetExpired,
                       base::Unretained(this)));
  }

  void OnRafBudgetExpired() {
    // Even though the rAF made a change to the frame's background color, no
    // further CompositorFrames should have been produced for animations,
    // because update_display_for_animations is false.
    EXPECT_EQ(1 + kNumFrames, begin_frame_counter_->begin_frame_count());
    EXPECT_EQ(1, render_frame_submission_observer_->render_frame_count());

    // Get animation frame count.
    devtools_client_->GetRuntime()->Evaluate(
        "window.rafCount",
        base::BindRepeating(&CompositorControllerRafBrowserTest::OnGetRafCount,
                            base::Unretained(this)));
  }

  void OnGetRafCount(std::unique_ptr<runtime::EvaluateResult> result) {
    EXPECT_NE(nullptr, result);
    EXPECT_FALSE(result->HasExceptionDetails());

    EXPECT_EQ(kNumFrames, result->GetResult()->GetValue()->GetInt());

    CaptureDoubleScreenshot(
        base::BindRepeating(&CompositorControllerRafBrowserTest::OnScreenshot,
                            base::Unretained(this)));
  }

  void OnScreenshot(const std::string& screenshot_data) {
    // Screenshot should have incurred two new CompositorFrames from renderer.
    EXPECT_EQ(3 + kNumFrames, begin_frame_counter_->begin_frame_count());
    EXPECT_EQ(3, render_frame_submission_observer_->render_frame_count());

    EXPECT_LT(0U, screenshot_data.length());

    if (screenshot_data.length()) {
      SkBitmap result_bitmap;
      EXPECT_TRUE(DecodePNG(screenshot_data, &result_bitmap));

      EXPECT_EQ(800, result_bitmap.width());
      EXPECT_EQ(600, result_bitmap.height());
      SkColor actual_color = result_bitmap.getColor(200, 200);
      // Screenshot was the fifth frame, so background color should be #500.
      SkColor expected_color = SkColorSetRGB(0x55, 0x00, 0x00);
      EXPECT_EQ(expected_color, actual_color);
    }

    FinishCompositorControllerTest();
  }

  static constexpr int kNumFrames = 3;
};

/* static */
constexpr int CompositorControllerRafBrowserTest::kNumFrames;

HEADLESS_ASYNC_DEVTOOLED_TEST_P(CompositorControllerRafBrowserTest);

// Instantiate test case for both software and gpu compositing modes.
INSTANTIATE_TEST_CASE_P(CompositorControllerRafBrowserTests,
                        CompositorControllerRafBrowserTest,
                        ::testing::Bool());

// Loads an animated GIF and verifies that:
// - animate_only BeginFrames don't produce CompositorFrames,
// - first screenshot starts the GIF animation,
// - animation is advanced according to virtual time.
//
// TODO(eseckler): Also verify that image animation frame times are not
// re-synced after the first iteration. This requires adding an flag/setting to
// disable such resyncs.
class CompositorControllerImageAnimationBrowserTest
    : public CompositorControllerBrowserTest {
 private:
  base::TimeDelta GetAnimationFrameInterval() const override {
    return base::TimeDelta::FromMilliseconds(500);
  }

  std::string GetTestFile() const override {
    // GIF: 1 second blue, 1 second red, 1 second yellow (100x100px).
    return "/animated_gif.html";
  }

  void OnFirstBeginFrameComplete() override {
    CompositorControllerBrowserTest::OnFirstBeginFrameComplete();

    // Post a task to grant more virtual time as we can't do this synchronously
    // from within VirtualTimeStopped().
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&CompositorControllerImageAnimationBrowserTest::
                           GrantFirstIterationBudget,
                       base::Unretained(this)));
  }

  void GrantFirstIterationBudget() {
    // AdditionalVirtualTimeBudget will self delete.
    new AdditionalVirtualTimeBudget(
        virtual_time_controller_.get(),
        AdditionalVirtualTimeBudget::StartPolicy::START_IMMEDIATELY,
        kNumFramesFirstIteration * GetAnimationFrameInterval(),
        base::BindOnce(&CompositorControllerImageAnimationBrowserTest::
                           OnFirstBudgetExpired,
                       base::Unretained(this)));
  }

  void OnFirstBudgetExpired() {
    // The GIF should not have started animating yet, even though we advanced
    // virtual time. It only starts animating when first painted, i.e. when the
    // first screenshot is taken.
    EXPECT_EQ(1 + kNumFramesFirstIteration,
              begin_frame_counter_->begin_frame_count());
    EXPECT_EQ(1, render_frame_submission_observer_->render_frame_count());

    CaptureDoubleScreenshot(
        base::BindRepeating(&CompositorControllerImageAnimationBrowserTest::
                                OnScreenshotAfterFirstIteration,
                            base::Unretained(this)));
  }

  void OnScreenshotAfterFirstIteration(const std::string& screenshot_data) {
    // Screenshot should have incurred one new CompositorFrame from renderer,
    // but two new BeginFrames.
    EXPECT_EQ(3 + kNumFramesFirstIteration,
              begin_frame_counter_->begin_frame_count());
    EXPECT_EQ(2, render_frame_submission_observer_->render_frame_count());

    EXPECT_LT(0U, screenshot_data.length());

    if (screenshot_data.length()) {
      SkBitmap result_bitmap;
      EXPECT_TRUE(DecodePNG(screenshot_data, &result_bitmap));

      EXPECT_EQ(800, result_bitmap.width());
      EXPECT_EQ(600, result_bitmap.height());
      SkColor actual_color = result_bitmap.getColor(50, 50);
      // Animation starts when first screenshot is taken, so should be blue.
      SkColor expected_color = SkColorSetRGB(0x00, 0x00, 0xff);
      EXPECT_EQ(expected_color, actual_color);
    }

    // Advance another iteration and check again that no CompositorFrames are
    // produced. AdditionalVirtualTimeBudget will self delete.
    new AdditionalVirtualTimeBudget(
        virtual_time_controller_.get(),
        AdditionalVirtualTimeBudget::StartPolicy::START_IMMEDIATELY,
        kNumFramesSecondIteration * GetAnimationFrameInterval(),
        base::BindOnce(&CompositorControllerImageAnimationBrowserTest::
                           OnSecondBudgetExpired,
                       base::Unretained(this)));
  }

  void OnSecondBudgetExpired() {
    // Even though the GIF animated, no further CompositorFrames should have
    // been produced, because update_display_for_animations is false. The second
    // animation only produces kNumFramesSecondIteration - 1 BeginFrames since
    // the first animation frame is skipped because of the prior screenshot.
    EXPECT_EQ(2 + kNumFramesFirstIteration + kNumFramesSecondIteration,
              begin_frame_counter_->begin_frame_count());
    EXPECT_EQ(2, render_frame_submission_observer_->render_frame_count());

    CaptureDoubleScreenshot(
        base::BindRepeating(&CompositorControllerImageAnimationBrowserTest::
                                OnScreenshotAfterSecondIteration,
                            base::Unretained(this)));
  }

  void OnScreenshotAfterSecondIteration(const std::string& screenshot_data) {
    // Screenshot should have incurred one new CompositorFrame from renderer,
    // but two new BeginFrames.
    EXPECT_EQ(4 + kNumFramesFirstIteration + kNumFramesSecondIteration,
              begin_frame_counter_->begin_frame_count());
    EXPECT_EQ(3, render_frame_submission_observer_->render_frame_count());

    EXPECT_LT(0U, screenshot_data.length());

    if (screenshot_data.length()) {
      SkBitmap result_bitmap;
      EXPECT_TRUE(DecodePNG(screenshot_data, &result_bitmap));

      EXPECT_EQ(800, result_bitmap.width());
      EXPECT_EQ(600, result_bitmap.height());
      SkColor actual_color = result_bitmap.getColor(50, 50);
      // We advanced two animation frames, so animation should now be yellow.
      SkColor expected_color = SkColorSetRGB(0xff, 0xff, 0x00);
      EXPECT_EQ(expected_color, actual_color);
    }

    FinishCompositorControllerTest();
  }

  // Enough to cover a full animation iteration.
  static constexpr int kNumFramesFirstIteration = 7;
  // Advances two animation frames only.
  static constexpr int kNumFramesSecondIteration = 5;
};

/* static */
constexpr int
    CompositorControllerImageAnimationBrowserTest::kNumFramesFirstIteration;
/* static */
constexpr int
    CompositorControllerImageAnimationBrowserTest::kNumFramesSecondIteration;

HEADLESS_ASYNC_DEVTOOLED_TEST_P(CompositorControllerImageAnimationBrowserTest);

// Instantiate test case for both software and gpu compositing modes.
INSTANTIATE_TEST_CASE_P(CompositorControllerImageAnimationBrowserTests,
                        CompositorControllerImageAnimationBrowserTest,
                        ::testing::Bool());

#endif  // !defined(OS_MACOSX)

}  // namespace headless
