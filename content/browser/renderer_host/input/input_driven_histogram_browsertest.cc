// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/platform/web_input_event.h"

using blink::WebInputEvent;

namespace {

// The event listener blocks the renderer's main thread on touchstart.
// This leads to the compositor impl thread to perform scrolling.
const char kURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<style>"
    ".spacer { height: 10000px; }"
    "</style>"
    "<div class=spacer></div>"
    "<script>"
    "  document.title='ready';"
    "</script>";
}  // namespace

namespace content {

class InputDrivenHistogramBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  InputDrivenHistogramBrowserTest() {}
  ~InputDrivenHistogramBrowserTest() override {}

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    runner_->Quit();
  }

 protected:
  // ContentBrowserTest
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (!GetParam()) {
      command_line->AppendSwitch(switches::kDisableThreadedScrolling);
    }
  }

  bool ThreadedScrollingEnabled() { return GetParam(); }

  void LoadURL(const char* test_url) {
    const GURL data_url(test_url);
    NavigateToURL(shell(), data_url);

    RenderWidgetHostImpl* host = GetWidgetHost();
    frame_observer_ = std::make_unique<RenderFrameSubmissionObserver>(
        host->render_frame_metadata_provider());
    host->GetView()->SetSize(gfx::Size(400, 400));

    base::string16 ready_title(base::ASCIIToUTF16("ready"));
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    ignore_result(watcher.WaitAndGetTitle());

    // We need to wait until at least one frame has been submitted
    // otherwise the injection of the synthetic gestures may get
    // dropped because of MainThread/Impl thread sync of touch event
    // regions.
    frame_observer_->WaitForAnyFrameSubmission();
  }

  // ContentBrowserTest:
  void PostRunTestOnMainThread() override {
    // Delete this before the WebContents is destroyed.
    frame_observer_.reset();
    ContentBrowserTest::PostRunTestOnMainThread();
  }

  int ExecuteScriptAndExtractInt(const std::string& script) {
    int value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        shell(), "domAutomationController.send(" + script + ")", &value));
    return value;
  }

  std::string ExecuteScriptAndExtractString(const std::string& script) {
    std::string output;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        shell(), "domAutomationController.send(" + script + ")", &output));
    return output;
  }

  int GetScrollTop() {
    return ExecuteScriptAndExtractInt("document.scrollingElement.scrollTop");
  }

  void ChangeBackgroundToGreen() {
    std::string script = "document.body.style.backgroundColor = 'green'";
    EXPECT_TRUE(content::ExecuteScript(shell(), script));
  }

  std::string GetBackgroundColor() {
    return ExecuteScriptAndExtractString("document.body.style.backgroundColor");
  }

  // Generate gesture events for a synthetic scroll from |point| for |distance|.
  void DoGestureScroll(const gfx::Point& point, const gfx::Vector2d& distance) {
    SyntheticSmoothScrollGestureParams params;
    params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
    params.anchor = gfx::PointF(point);
    params.distances.push_back(-distance);

    runner_.reset(new base::RunLoop);

    std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
        new SyntheticSmoothScrollGesture(params));
    GetWidgetHost()->QueueSyntheticGesture(
        std::move(gesture),
        base::BindOnce(
            &InputDrivenHistogramBrowserTest::OnSyntheticGestureCompleted,
            base::Unretained(this)));

    // Runs until we get the OnSyntheticGestureCompleted callback
    runner_->Run();
    runner_.reset();

    // Wait until the scroll is done. The result of using synthetic gesture
    // scroll may not be as accurate as the required amount. Here we allow 1px
    // of inaccuracy.
    gfx::Vector2dF default_scroll_offset;
    while (frame_observer_->LastRenderFrameMetadata()
               .root_scroll_offset.value_or(default_scroll_offset)
               .y() <= (distance.y() - 1)) {
      frame_observer_->WaitForMetadataChange();
    }

    EXPECT_GE(GetScrollTop(), distance.y() - 1);
  }

  void ReadHistograms(const base::HistogramTester& histogram_tester,
                      base::HistogramBase::Count* total_impl_samples_count,
                      base::HistogramBase::Count* total_main_samples_count) {
    content::FetchHistogramsFromChildProcesses();

    std::vector<base::Bucket> all_impl_samples = histogram_tester.GetAllSamples(
        "Scheduling.Renderer.DrawIntervalWithCompositedAnimations2");
    for (const auto& it : all_impl_samples)
      *total_impl_samples_count += it.count;

    std::vector<base::Bucket> all_main_samples = histogram_tester.GetAllSamples(
        "Scheduling.Renderer.DrawIntervalWithMainThreadAnimations2");
    for (const auto& it : all_main_samples)
      *total_main_samples_count += it.count;
  }

 private:
  std::unique_ptr<RenderFrameSubmissionObserver> frame_observer_;
  std::unique_ptr<base::RunLoop> runner_;

  DISALLOW_COPY_AND_ASSIGN(InputDrivenHistogramBrowserTest);
};

// Mac doesn't yet have a gesture recognizer, so can't support turning touch
// events into scroll gestures.
#if defined(OS_MACOSX)
#define MAYBE_NoInput DISABLED_NoInput
#else
#define MAYBE_NoInput NoInput
#endif
IN_PROC_BROWSER_TEST_P(InputDrivenHistogramBrowserTest, MAYBE_NoInput) {
  base::HistogramTester histogram_tester;
  base::HistogramBase::Count total_impl_samples_count = 0;
  base::HistogramBase::Count total_main_samples_count = 0;
  LoadURL(kURL);
  ReadHistograms(histogram_tester, &total_impl_samples_count,
                 &total_main_samples_count);
  EXPECT_EQ(total_impl_samples_count, 0);
  EXPECT_EQ(total_main_samples_count, 0);

  ChangeBackgroundToGreen();
  std::string background_color = GetBackgroundColor();
  EXPECT_EQ(background_color, "green");

  ReadHistograms(histogram_tester, &total_impl_samples_count,
                 &total_main_samples_count);
  EXPECT_EQ(total_impl_samples_count, 0);
  EXPECT_EQ(total_main_samples_count, 0);
}

#if defined(OS_MACOSX)
#define MAYBE_ScrollPageWithTouch DISABLED_ScrollPageWithTouch
#else
#define MAYBE_ScrollPageWithTouch ScrollPageWithTouch
#endif
IN_PROC_BROWSER_TEST_P(InputDrivenHistogramBrowserTest,
                       MAYBE_ScrollPageWithTouch) {
  base::HistogramTester histogram_tester;
  base::HistogramBase::Count total_impl_samples_count = 0;
  base::HistogramBase::Count total_main_samples_count = 0;
  LoadURL(kURL);
  DoGestureScroll(gfx::Point(50, 50), gfx::Vector2d(0, 45));
  ReadHistograms(histogram_tester, &total_impl_samples_count,
                 &total_main_samples_count);
  if (ThreadedScrollingEnabled()) {
    EXPECT_GT(total_impl_samples_count, 0);
    EXPECT_EQ(total_main_samples_count, 0);
  } else {
    EXPECT_EQ(total_impl_samples_count, 0);
    EXPECT_GT(total_main_samples_count, 0);
  }
}

#if defined(OS_MACOSX)
#define MAYBE_ScrollFollowedByNoInput DISABLED_ScrollFollowedByNoInput
#else
#define MAYBE_ScrollFollowedByNoInput ScrollFollowedByNoInput
#endif
IN_PROC_BROWSER_TEST_P(InputDrivenHistogramBrowserTest,
                       MAYBE_ScrollFollowedByNoInput) {
  base::HistogramTester histogram_tester;
  base::HistogramBase::Count total_impl_samples_count = 0;
  base::HistogramBase::Count total_main_samples_count = 0;
  LoadURL(kURL);
  DoGestureScroll(gfx::Point(50, 50), gfx::Vector2d(0, 45));
  ReadHistograms(histogram_tester, &total_impl_samples_count,
                 &total_main_samples_count);
  if (ThreadedScrollingEnabled()) {
    EXPECT_GT(total_impl_samples_count, 0);
    EXPECT_EQ(total_main_samples_count, 0);
  } else {
    EXPECT_EQ(total_impl_samples_count, 0);
    EXPECT_GT(total_main_samples_count, 0);
  }

  ChangeBackgroundToGreen();
  std::string background_color = GetBackgroundColor();
  EXPECT_EQ(background_color, "green");

  base::HistogramBase::Count new_total_impl_samples_count = 0;
  base::HistogramBase::Count new_total_main_samples_count = 0;
  ReadHistograms(histogram_tester, &new_total_impl_samples_count,
                 &new_total_main_samples_count);
  EXPECT_EQ(new_total_impl_samples_count, total_impl_samples_count);
  EXPECT_EQ(new_total_main_samples_count, total_main_samples_count);
}

INSTANTIATE_TEST_CASE_P(All,
                        InputDrivenHistogramBrowserTest,
                        testing::Values(true, false));

}  // namespace content
