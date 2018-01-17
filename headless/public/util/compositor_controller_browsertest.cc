// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/compositor_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
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
static constexpr base::TimeDelta kWaitForCompositorReadyFrameDelay =
    base::TimeDelta::FromMilliseconds(20);

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
      bool main_frame_content_updated;
      if (result_dict->GetBoolean("mainFrameContentUpdated",
                                  &main_frame_content_updated)) {
        ++begin_frame_count_;
        if (main_frame_content_updated)
          ++main_frame_update_count_;
      }
    }
    return false;
  }

  int begin_frame_count() const { return begin_frame_count_; }
  int main_frame_update_count() const { return main_frame_update_count_; }

 private:
  HeadlessDevToolsClient* client_;  // NOT OWNED.
  int begin_frame_count_ = 0;
  int main_frame_update_count_ = 0;
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
    command_line->AppendSwitch(cc::switches::kRunAllCompositorStagesBeforeDraw);
    command_line->AppendSwitch(switches::kDisableNewContentRenderingTimeout);
    command_line->AppendSwitch(cc::switches::kDisableCheckerImaging);
    command_line->AppendSwitch(cc::switches::kDisableThreadedAnimation);
    command_line->AppendSwitch(switches::kDisableThreadedScrolling);
  }

  bool GetEnableBeginFrameControl() override { return true; }

  void RunDevTooledTest() override {
    begin_frame_counter_ =
        base::MakeUnique<BeginFrameCounter>(devtools_client_.get());
    virtual_time_controller_ =
        std::make_unique<VirtualTimeController>(devtools_client_.get());
    const bool update_display_for_animations = false;
    compositor_controller_ = base::MakeUnique<CompositorController>(
        browser()->BrowserMainThread(), devtools_client_.get(),
        virtual_time_controller_.get(), kAnimationFrameInterval,
        kWaitForCompositorReadyFrameDelay, update_display_for_animations);

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CompositorControllerBrowserTest::WaitForReady,
                       base::Unretained(this)),
        base::TimeDelta::FromSeconds(1));
  }

  void WaitForReady() {
    compositor_controller_->WaitForCompositorReady(
        base::BindRepeating(&CompositorControllerBrowserTest::OnCompositorReady,
                            base::Unretained(this)));
  }

  virtual void OnCompositorReadyExpectations() {
    // The renderer's first CompositorFrame may or may not have been included in
    // a BeginFrame result.
    main_frame_update_count_after_ready_ =
        begin_frame_counter_->main_frame_update_count();
    EXPECT_GE(1, main_frame_update_count_after_ready_);
  }

  void OnCompositorReady() {
    OnCompositorReadyExpectations();

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

    virtual_time_controller_->GrantVirtualTimeBudget(
        emulation::VirtualTimePolicy::ADVANCE,
        kNumFrames * kAnimationFrameInterval, base::Bind([]() {}),
        base::BindRepeating(
            &CompositorControllerBrowserTest::OnVirtualTimeBudgetExpired,
            base::Unretained(this)));
  }

  void OnVirtualTimeBudgetExpired() {
    // Even though the rAF made a change to the frame's background color, no
    // further CompositorFrames should have been produced for animations,
    // because update_display_for_animations is false.
    EXPECT_EQ(main_frame_update_count_after_ready_,
              begin_frame_counter_->main_frame_update_count());

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

    FinishAsynchronousTest();
  }

 protected:
  static constexpr int kNumFrames = 3;

  std::unique_ptr<BeginFrameCounter> begin_frame_counter_;
  std::unique_ptr<VirtualTimeController> virtual_time_controller_;
  std::unique_ptr<CompositorController> compositor_controller_;
  int main_frame_update_count_after_ready_ = 0;
};

/* static */
constexpr int CompositorControllerBrowserTest::kNumFrames;

HEADLESS_ASYNC_DEVTOOLED_TEST_P(CompositorControllerBrowserTest);

// Instantiate test case for both software and gpu compositing modes.
INSTANTIATE_TEST_CASE_P(CompositorControllerBrowserTests,
                        CompositorControllerBrowserTest,
                        ::testing::Bool());

class CompositorControllerSurfaceSyncBrowserTest
    : public CompositorControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    CompositorControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableSurfaceSynchronization);
  }

  void OnCompositorReadyExpectations() override {
    CompositorControllerBrowserTest::OnCompositorReadyExpectations();
    // With surface sync enabled, we should have waited for the renderer's
    // CompositorFrame in the first BeginFrame.
    EXPECT_EQ(1, begin_frame_counter_->begin_frame_count());
    EXPECT_EQ(1, begin_frame_counter_->main_frame_update_count());
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_P(CompositorControllerSurfaceSyncBrowserTest);

// Instantiate test case for both software and gpu compositing modes.
INSTANTIATE_TEST_CASE_P(CompositorControllerSurfaceSyncBrowserTests,
                        CompositorControllerSurfaceSyncBrowserTest,
                        ::testing::Bool());

#endif  // !defined(OS_MACOSX)

}  // namespace headless
