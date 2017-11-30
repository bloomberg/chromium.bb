// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/render_widget_host_latency_tracker.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/histogram_tester.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/test_rappor_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using blink::WebInputEvent;
using testing::ElementsAre;

namespace content {
namespace {

// Trace ids are generated in sequence in practice, but in these tests, we don't
// care about the value, so we'll just use a constant.
const int kTraceEventId = 5;
const char kUrl[] = "http://www.foo.bar.com/subpage/1";

void AddFakeComponentsWithTimeStamp(
    const RenderWidgetHostLatencyTracker& tracker,
    ui::LatencyInfo* latency,
    base::TimeTicks time_stamp) {
  latency->AddLatencyNumberWithTimestamp(ui::INPUT_EVENT_LATENCY_UI_COMPONENT,
                                         0, 0, time_stamp, 1);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0, time_stamp,
      1);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0, time_stamp, 1);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, 0, 0, time_stamp, 1);
  latency->AddLatencyNumberWithTimestamp(
      ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, 0, 0, time_stamp, 1);
}

void AddFakeComponents(const RenderWidgetHostLatencyTracker& tracker,
                       ui::LatencyInfo* latency) {
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker.latency_component_id(), 0, base::TimeTicks::Now(), 1);
  AddFakeComponentsWithTimeStamp(tracker, latency, base::TimeTicks::Now());
}

void AddRenderingScheduledComponent(ui::LatencyInfo* latency,
                                    bool main,
                                    base::TimeTicks time_stamp) {
  if (main) {
    latency->AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT, 0, 0,
        time_stamp, 1);

  } else {
    latency->AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT, 0, 0,
        time_stamp, 1);
  }
}

}  // namespace

class RenderWidgetHostLatencyTrackerTestBrowserClient
    : public TestContentBrowserClient {
 public:
  RenderWidgetHostLatencyTrackerTestBrowserClient() {}
  ~RenderWidgetHostLatencyTrackerTestBrowserClient() override {}

  rappor::RapporService* GetRapporService() override {
    return &rappor_service_;
  }

  rappor::TestRapporServiceImpl* getTestRapporService() {
    return &rappor_service_;
  }

  ukm::TestUkmRecorder* GetTestUkmRecorder() { return &test_ukm_recorder_; }

 private:
  rappor::TestRapporServiceImpl rappor_service_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostLatencyTrackerTestBrowserClient);
};

class RenderWidgetHostLatencyTrackerTest
    : public RenderViewHostImplTestHarness {
 public:
  RenderWidgetHostLatencyTrackerTest() : old_browser_client_(nullptr) {
    ResetHistograms();
  }

  ::testing::AssertionResult RapporSampleAssert(const char* rappor_name,
                                                int count) {
    rappor::TestSample::Shadow* sample_obj =
        test_browser_client_.getTestRapporService()->GetRecordedSampleForMetric(
            rappor_name);
    if (count) {
      if (!sample_obj)
        return ::testing::AssertionFailure()
               << rappor_name << " rappor sample should not be null";

      const auto& domain_it = sample_obj->string_fields.find("Domain");
      if (domain_it == sample_obj->string_fields.end())
        return ::testing::AssertionFailure()
               << rappor_name << " rappor sample should contain the string "
                                 "attribute \"Domain\"";
      const auto& domain = domain_it->second;
      if (domain != "bar.com")
        return ::testing::AssertionFailure()
               << rappor_name << " rappor expected bar.com domain but had "
               << domain << " domain";

      const auto& latency_it = sample_obj->uint64_fields.find("Latency");
      if (latency_it == sample_obj->uint64_fields.end())
        return ::testing::AssertionFailure()
               << rappor_name << " rappor sample should contain the uint64 "
                                 "attribute \"Latency\"";
      const auto& latency_noise = latency_it->second.second;
      if (latency_noise != rappor::NO_NOISE)
        return ::testing::AssertionFailure()
               << rappor_name
               << " rappor expected rappor::NO_NOISE latency but had "
               << latency_noise << " latency";

      return ::testing::AssertionSuccess();
    } else {
      if (!sample_obj)
        return ::testing::AssertionSuccess();
      else
        return ::testing::AssertionFailure() << rappor_name
                                             << " rappor sample should be null";
    }
  }

  void ExpectUkmReported(const char* event_name,
                         const char* metric_name,
                         size_t expected_count) {
    const ukm::TestUkmRecorder* ukm_recoder =
        test_browser_client_.GetTestUkmRecorder();

    auto entries = ukm_recoder->GetEntriesByName(event_name);
    EXPECT_EQ(expected_count, entries.size());
    for (const auto* const entry : entries) {
      ukm_recoder->ExpectEntrySourceHasUrl(entry, GURL(kUrl));
      EXPECT_TRUE(ukm_recoder->EntryHasMetric(entry, metric_name));
    }
  }

  ::testing::AssertionResult HistogramSizeEq(const char* histogram_name,
                                             int size) {
    uint64_t histogram_size =
        histogram_tester_->GetAllSamples(histogram_name).size();
    if (static_cast<uint64_t>(size) == histogram_size) {
      return ::testing::AssertionSuccess();
    } else {
      return ::testing::AssertionFailure() << histogram_name << " expected "
                                           << size << " entries, but had "
                                           << histogram_size;
    }
  }

  RenderWidgetHostLatencyTracker* tracker() { return tracker_.get(); }
  void ResetHistograms() {
    histogram_tester_.reset(new base::HistogramTester());
  }

  const base::HistogramTester& histogram_tester() {
    return *histogram_tester_;
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    old_browser_client_ = SetBrowserClientForTesting(&test_browser_client_);
    tracker_ =
        std::make_unique<RenderWidgetHostLatencyTracker>(false, contents());
    tracker_->Initialize(kTestRoutingId, kTestProcessId);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_browser_client_);
    RenderViewHostImplTestHarness::TearDown();
    test_browser_client_.GetTestUkmRecorder()->Purge();
  }

 protected:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostLatencyTrackerTest);
  const int kTestRoutingId = 3;
  const int kTestProcessId = 1;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<RenderWidgetHostLatencyTracker> tracker_;
  RenderWidgetHostLatencyTrackerTestBrowserClient test_browser_client_;
  ContentBrowserClient* old_browser_client_;
};

TEST_F(RenderWidgetHostLatencyTrackerTest, TestValidEventTiming) {
  base::TimeTicks now = base::TimeTicks::Now();

  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(kTraceEventId);
  latency_info.set_source_event_type(ui::SourceEventType::WHEEL);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT, 0, 0,
      now + base::TimeDelta::FromMilliseconds(60), 1);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT, 0, 0,
      now + base::TimeDelta::FromMilliseconds(50), 1);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, 0, 0,
      now + base::TimeDelta::FromMilliseconds(40), 1);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, 0, 0,
      now + base::TimeDelta::FromMilliseconds(30), 1);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      tracker()->latency_component_id(), 0,
      now + base::TimeDelta::FromMilliseconds(20), 1);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0,
      now + base::TimeDelta::FromMilliseconds(10), 1);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0, now, 1);

  tracker()->OnGpuSwapBuffersCompleted(latency_info);

  // When last_event_time of the end_component is less than the first_event_time
  // of the start_component, zero is recorded instead of a negative value.
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.TimeToScrollUpdateSwapBegin2", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.Scroll.Wheel.TimeToScrollUpdateSwapBegin2", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.TimeToHandled2_Impl", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.Scroll.Wheel.TimeToHandled2_Impl", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.HandledToRendererSwap2_Impl", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.RendererSwapToBrowserNotified2", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.BrowserNotifiedToBeforeGpuSwap2", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.GpuSwap2", 0, 1);
}

TEST_F(RenderWidgetHostLatencyTrackerTest, TestWheelToFirstScrollHistograms) {
  const GURL url(kUrl);
  size_t total_ukm_entry_count = 0;
  contents()->NavigateAndCommit(url);
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
          blink::WebMouseWheelEvent::kPhaseChanged);
      base::TimeTicks now = base::TimeTicks::Now();
      wheel.SetTimeStampSeconds((now - base::TimeTicks()).InSecondsF());
      ui::LatencyInfo wheel_latency(ui::SourceEventType::WHEEL);
      wheel_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          tracker()->latency_component_id(), 0, now, 1);
      AddFakeComponentsWithTimeStamp(*tracker(), &wheel_latency, now);
      AddRenderingScheduledComponent(&wheel_latency, rendering_on_main, now);
      tracker()->OnInputEvent(wheel, &wheel_latency);
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), nullptr));
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      tracker()->OnInputEventAck(wheel, &wheel_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      tracker()->OnGpuSwapBuffersCompleted(wheel_latency);

      // UKM metrics.
      total_ukm_entry_count++;
      ExpectUkmReported("Event.ScrollBegin.Wheel",
                        "TimeToScrollUpdateSwapBegin", total_ukm_entry_count);
      // Rappor metrics.
      EXPECT_TRUE(
          RapporSampleAssert("Event.Latency.ScrollUpdate.Touch."
                             "TimeToScrollUpdateSwapBegin2",
                             0));
      EXPECT_TRUE(
          RapporSampleAssert("Event.Latency.ScrollUpdate.Wheel."
                             "TimeToScrollUpdateSwapBegin2",
                             0));
      EXPECT_TRUE(
          RapporSampleAssert("Event.Latency.ScrollBegin.Touch."
                             "TimeToScrollUpdateSwapBegin2",
                             0));
      EXPECT_TRUE(
          RapporSampleAssert("Event.Latency.ScrollBegin.Wheel."
                             "TimeToScrollUpdateSwapBegin2",
                             2));
      EXPECT_EQ(2,
                test_browser_client_.getTestRapporService()->GetReportsCount());

      // UMA histograms.
      EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.WheelUI", 1));
      EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.WheelAcked", 1));

      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "TimeToScrollUpdateSwapBegin2",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel."
                          "TimeToScrollUpdateSwapBegin2",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollUpdate.Wheel."
                          "TimeToScrollUpdateSwapBegin2",
                          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel.TimeToHandled2_Main",
                          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel.TimeToHandled2_Impl",
                          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel.TimeToHandled2_Main",
                          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel.TimeToHandled2_Impl",
                          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.HandledToRendererSwap2_Main",
          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.HandledToRendererSwap2_Impl",
          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "RendererSwapToBrowserNotified2",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "BrowserNotifiedToBeforeGpuSwap2",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel.GpuSwap2", 1));

      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.TimeToHandled2_Main", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.TimeToHandled2_Impl", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.HandledToRendererSwap2_Main", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.HandledToRendererSwap2_Impl", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.RendererSwapToBrowserNotified2",
          0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.BrowserNotifiedToBeforeGpuSwap2",
          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollUpdate.Wheel.GpuSwap2", 0));
    }
  }
}

TEST_F(RenderWidgetHostLatencyTrackerTest, TestWheelToScrollHistograms) {
  const GURL url(kUrl);
  size_t total_ukm_entry_count = 0;
  contents()->NavigateAndCommit(url);
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
          blink::WebMouseWheelEvent::kPhaseChanged);
      base::TimeTicks now = base::TimeTicks::Now();
      wheel.SetTimeStampSeconds((now - base::TimeTicks()).InSecondsF());
      ui::LatencyInfo wheel_latency(ui::SourceEventType::WHEEL);
      wheel_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          tracker()->latency_component_id(), 0, now, 1);
      AddFakeComponentsWithTimeStamp(*tracker(), &wheel_latency, now);
      AddRenderingScheduledComponent(&wheel_latency, rendering_on_main, now);
      tracker()->OnInputEvent(wheel, &wheel_latency);
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), nullptr));
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      tracker()->OnInputEventAck(wheel, &wheel_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      tracker()->OnGpuSwapBuffersCompleted(wheel_latency);

      // UKM metrics.
      total_ukm_entry_count++;
      ExpectUkmReported("Event.ScrollUpdate.Wheel",
                        "TimeToScrollUpdateSwapBegin", total_ukm_entry_count);
      // Rappor metrics.
      EXPECT_TRUE(
          RapporSampleAssert("Event.Latency.ScrollUpdate.Touch."
                             "TimeToScrollUpdateSwapBegin2",
                             0));
      EXPECT_TRUE(
          RapporSampleAssert("Event.Latency.ScrollUpdate.Wheel."
                             "TimeToScrollUpdateSwapBegin2",
                             2));
      EXPECT_TRUE(
          RapporSampleAssert("Event.Latency.ScrollBegin.Touch."
                             "TimeToScrollUpdateSwapBegin2",
                             0));
      EXPECT_TRUE(
          RapporSampleAssert("Event.Latency.ScrollBegin.Wheel."
                             "TimeToScrollUpdateSwapBegin2",
                             0));
      EXPECT_EQ(2,
                test_browser_client_.getTestRapporService()->GetReportsCount());

      // UMA histograms.
      EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.WheelUI", 1));
      EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.WheelAcked", 1));

      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "TimeToScrollUpdateSwapBegin2",
                          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel."
                          "TimeToScrollUpdateSwapBegin2",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollUpdate.Wheel."
                          "TimeToScrollUpdateSwapBegin2",
                          1));

      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.TimeToHandled2_Main", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.TimeToHandled2_Impl", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.HandledToRendererSwap2_Main", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.HandledToRendererSwap2_Impl", 0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "RendererSwapToBrowserNotified2",
                          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "BrowserNotifiedToBeforeGpuSwap2",
                          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel.GpuSwap2", 0));

      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.TimeToHandled2_Main",
          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.TimeToHandled2_Impl",
          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel.TimeToHandled2_Main",
                          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel.TimeToHandled2_Impl",
                          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.HandledToRendererSwap2_Main",
          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.HandledToRendererSwap2_Impl",
          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.RendererSwapToBrowserNotified2",
          1));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.BrowserNotifiedToBeforeGpuSwap2",
          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollUpdate.Wheel.GpuSwap2", 1));
    }
  }
}

TEST_F(RenderWidgetHostLatencyTrackerTest, TestTouchToFirstScrollHistograms) {
  const GURL url(kUrl);
  contents()->NavigateAndCommit(url);
  size_t total_ukm_entry_count = 0;
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto scroll = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, blink::kWebGestureDeviceTouchscreen);
      base::TimeTicks now = base::TimeTicks::Now();
      scroll.SetTimeStampSeconds((now - base::TimeTicks()).InSecondsF());
      ui::LatencyInfo scroll_latency;
      scroll_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          tracker()->latency_component_id(), 0, now, 1);
      AddFakeComponentsWithTimeStamp(*tracker(), &scroll_latency, now);
      AddRenderingScheduledComponent(&scroll_latency, rendering_on_main, now);
      tracker()->OnInputEvent(scroll, &scroll_latency);
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), nullptr));
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      tracker()->OnInputEventAck(scroll, &scroll_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    }

    {
      SyntheticWebTouchEvent touch;
      touch.PressPoint(0, 0);
      touch.PressPoint(1, 1);
      ui::LatencyInfo touch_latency(ui::SourceEventType::TOUCH);
      base::TimeTicks now = base::TimeTicks::Now();
      touch_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          tracker()->latency_component_id(), 0, now, 1);
      AddFakeComponentsWithTimeStamp(*tracker(), &touch_latency, now);
      AddRenderingScheduledComponent(&touch_latency, rendering_on_main, now);
      tracker()->OnInputEvent(touch, &touch_latency);
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), nullptr));
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      tracker()->OnInputEventAck(touch, &touch_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      tracker()->OnGpuSwapBuffersCompleted(touch_latency);
    }

    // UKM metrics.
    total_ukm_entry_count++;
    ExpectUkmReported("Event.ScrollBegin.Touch", "TimeToScrollUpdateSwapBegin",
                      total_ukm_entry_count);
    // Rappor metrics.
    EXPECT_TRUE(
        RapporSampleAssert("Event.Latency.ScrollUpdate.Touch."
                           "TimeToScrollUpdateSwapBegin2",
                           0));
    EXPECT_TRUE(
        RapporSampleAssert("Event.Latency.ScrollUpdate.Wheel."
                           "TimeToScrollUpdateSwapBegin2",
                           0));
    EXPECT_TRUE(
        RapporSampleAssert("Event.Latency.ScrollBegin.Touch."
                           "TimeToScrollUpdateSwapBegin2",
                           2));
    EXPECT_TRUE(
        RapporSampleAssert("Event.Latency.ScrollBegin.Wheel."
                           "TimeToScrollUpdateSwapBegin2",
                           0));
    EXPECT_EQ(2,
              test_browser_client_.getTestRapporService()->GetReportsCount());

    // UMA histograms.
    EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.TouchUI", 1));
    EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.TouchAcked", 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.TimeToScrollUpdateSwapBegin2", 1));

    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.TimeToScrollUpdateSwapBegin2", 0));

    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch.TimeToHandled2_Main",
                        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch.TimeToHandled2_Impl",
                        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.HandledToRendererSwap2_Main",
        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.HandledToRendererSwap2_Impl",
        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch."
                        "RendererSwapToBrowserNotified2",
                        1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch."
                        "BrowserNotifiedToBeforeGpuSwap2",
                        1));
    EXPECT_TRUE(HistogramSizeEq("Event.Latency.ScrollBegin.Touch.GpuSwap2", 1));

    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.TimeToHandled2_Main", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.TimeToHandled2_Impl", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.HandledToRendererSwap2_Main", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.HandledToRendererSwap2_Impl", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.RendererSwapToBrowserNotified2", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.BrowserNotifiedToBeforeGpuSwap2", 0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.Touch.GpuSwap2", 0));
  }
}

TEST_F(RenderWidgetHostLatencyTrackerTest, TestTouchToScrollHistograms) {
  const GURL url(kUrl);
  contents()->NavigateAndCommit(url);
  size_t total_ukm_entry_count = 0;
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    EXPECT_EQ(0,
              test_browser_client_.getTestRapporService()->GetReportsCount());
    {
      auto scroll = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, blink::kWebGestureDeviceTouchscreen);
      base::TimeTicks now = base::TimeTicks::Now();
      scroll.SetTimeStampSeconds((now - base::TimeTicks()).InSecondsF());
      ui::LatencyInfo scroll_latency;
      scroll_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          tracker()->latency_component_id(), 0, now, 1);
      AddFakeComponentsWithTimeStamp(*tracker(), &scroll_latency, now);
      AddRenderingScheduledComponent(&scroll_latency, rendering_on_main, now);
      tracker()->OnInputEvent(scroll, &scroll_latency);
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), nullptr));
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      tracker()->OnInputEventAck(scroll, &scroll_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    }

    {
      SyntheticWebTouchEvent touch;
      touch.PressPoint(0, 0);
      touch.PressPoint(1, 1);
      ui::LatencyInfo touch_latency(ui::SourceEventType::TOUCH);
      base::TimeTicks now = base::TimeTicks::Now();
      touch_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          tracker()->latency_component_id(), 0, now, 1);
      AddFakeComponentsWithTimeStamp(*tracker(), &touch_latency, now);
      AddRenderingScheduledComponent(&touch_latency, rendering_on_main, now);
      tracker()->OnInputEvent(touch, &touch_latency);
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), nullptr));
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      tracker()->OnInputEventAck(touch, &touch_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      tracker()->OnGpuSwapBuffersCompleted(touch_latency);
    }

    // UKM metrics.
    total_ukm_entry_count++;
    ExpectUkmReported("Event.ScrollUpdate.Touch", "TimeToScrollUpdateSwapBegin",
                      total_ukm_entry_count);

    // Rappor metrics.
    EXPECT_TRUE(
        RapporSampleAssert("Event.Latency.ScrollUpdate.Touch."
                           "TimeToScrollUpdateSwapBegin2",
                           2));
    EXPECT_TRUE(
        RapporSampleAssert("Event.Latency.ScrollUpdate.Wheel."
                           "TimeToScrollUpdateSwapBegin2",
                           0));
    EXPECT_TRUE(
        RapporSampleAssert("Event.Latency.ScrollBegin.Touch."
                           "TimeToScrollUpdateSwapBegin2",
                           0));
    EXPECT_TRUE(
        RapporSampleAssert("Event.Latency.ScrollBegin.Wheel."
                           "TimeToScrollUpdateSwapBegin2",
                           0));

    EXPECT_EQ(2,
              test_browser_client_.getTestRapporService()->GetReportsCount());

    // UMA histograms.
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.TimeToScrollUpdateSwapBegin2", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.TimeToScrollUpdateSwapBegin2", 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.TimeToHandled2_Main", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.TimeToHandled2_Impl", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.HandledToRendererSwap2_Main", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.HandledToRendererSwap2_Impl", 0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch."
                        "RendererSwapToBrowserNotified2",
                        0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch."
                        "BrowserNotifiedToBeforeGpuSwap2",
                        0));
    EXPECT_TRUE(HistogramSizeEq("Event.Latency.ScrollBegin.Touch.GpuSwap2", 0));

    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.Touch.TimeToHandled2_Main",
                        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.Touch.TimeToHandled2_Impl",
                        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.HandledToRendererSwap2_Main",
        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.HandledToRendererSwap2_Impl",
        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.RendererSwapToBrowserNotified2", 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.BrowserNotifiedToBeforeGpuSwap2", 1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.Touch.GpuSwap2", 1));
  }
}

TEST_F(RenderWidgetHostLatencyTrackerTest,
       LatencyTerminatedOnAckIfRenderingNotScheduled) {
  {
    auto scroll = SyntheticWebGestureEventBuilder::BuildScrollBegin(
        5.f, -5.f, blink::kWebGestureDeviceTouchscreen);
    ui::LatencyInfo scroll_latency;
    AddFakeComponents(*tracker(), &scroll_latency);
    // Don't include the rendering schedule component, since we're testing the
    // case where rendering isn't scheduled.
    tracker()->OnInputEvent(scroll, &scroll_latency);
    tracker()->OnInputEventAck(scroll, &scroll_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(scroll_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_NO_SWAP_COMPONENT, 0, nullptr));
    EXPECT_TRUE(scroll_latency.terminated());
  }

  {
    auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
        blink::WebMouseWheelEvent::kPhaseChanged);
    ui::LatencyInfo wheel_latency;
    wheel_latency.set_source_event_type(ui::SourceEventType::WHEEL);
    AddFakeComponents(*tracker(), &wheel_latency);
    tracker()->OnInputEvent(wheel, &wheel_latency);
    tracker()->OnInputEventAck(wheel, &wheel_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(wheel_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_NO_SWAP_COMPONENT, 0, nullptr));
    EXPECT_TRUE(wheel_latency.terminated());
  }

  {
    SyntheticWebTouchEvent touch;
    touch.PressPoint(0, 0);
    ui::LatencyInfo touch_latency;
    touch_latency.set_source_event_type(ui::SourceEventType::TOUCH);
    AddFakeComponents(*tracker(), &touch_latency);
    tracker()->OnInputEvent(touch, &touch_latency);
    tracker()->OnInputEventAck(touch, &touch_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(touch_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_NO_SWAP_COMPONENT, 0, nullptr));
    EXPECT_TRUE(touch_latency.terminated());
  }

  {
    auto mouse_move =
        SyntheticWebMouseEventBuilder::Build(blink::WebMouseEvent::kMouseMove);
    ui::LatencyInfo mouse_latency;
    AddFakeComponents(*tracker(), &mouse_latency);
    tracker()->OnInputEvent(mouse_move, &mouse_latency);
    tracker()->OnInputEventAck(mouse_move, &mouse_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(mouse_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_NO_SWAP_COMPONENT, 0, nullptr));
    EXPECT_TRUE(mouse_latency.terminated());
  }

  {
    auto key_event =
        SyntheticWebKeyboardEventBuilder::Build(blink::WebKeyboardEvent::kChar);
    ui::LatencyInfo key_latency;
    key_latency.set_source_event_type(ui::SourceEventType::KEY_PRESS);
    AddFakeComponents(*tracker(), &key_latency);
    tracker()->OnInputEvent(key_event, &key_latency);
    tracker()->OnInputEventAck(key_event, &key_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(key_latency.FindLatency(
        ui::INPUT_EVENT_LATENCY_TERMINATED_NO_SWAP_COMPONENT, 0, nullptr));
    EXPECT_TRUE(key_latency.terminated());
  }

  EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.WheelUI", 1));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.TouchUI", 1));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.WheelAcked", 1));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.Browser.TouchAcked", 1));
  EXPECT_TRUE(
      HistogramSizeEq("Event.Latency.ScrollUpdate.TouchToHandled_Main", 0));
  EXPECT_TRUE(
      HistogramSizeEq("Event.Latency.ScrollUpdate.TouchToHandled_Impl", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.HandledToRendererSwap_Main", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.HandledToRendererSwap_Impl", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.RendererSwapToBrowserNotified", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.BrowserNotifiedToBeforeGpuSwap", 0));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.ScrollUpdate.GpuSwap", 0));
}

TEST_F(RenderWidgetHostLatencyTrackerTest, ScrollLatency) {
  auto scroll_begin = SyntheticWebGestureEventBuilder::BuildScrollBegin(
      5, -5, blink::kWebGestureDeviceTouchscreen);
  ui::LatencyInfo scroll_latency;
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                                  0);
  tracker()->OnInputEvent(scroll_begin, &scroll_latency);
  EXPECT_TRUE(
      scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                 tracker()->latency_component_id(), nullptr));
  EXPECT_EQ(2U, scroll_latency.latency_components().size());

  // The first GestureScrollUpdate should be provided with
  // INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT.
  auto first_scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      5.f, -5.f, 0, blink::kWebGestureDeviceTouchscreen);
  scroll_latency = ui::LatencyInfo();
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                                  0);
  tracker()->OnInputEvent(first_scroll_update, &scroll_latency);
  EXPECT_TRUE(
      scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                 tracker()->latency_component_id(), nullptr));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker()->latency_component_id(), nullptr));
  EXPECT_FALSE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker()->latency_component_id(), nullptr));
  EXPECT_EQ(3U, scroll_latency.latency_components().size());

  // Subsequent GestureScrollUpdates should be provided with
  // INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT.
  auto scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      -5.f, 5.f, 0, blink::kWebGestureDeviceTouchscreen);
  scroll_latency = ui::LatencyInfo();
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                                  0);
  tracker()->OnInputEvent(scroll_update, &scroll_latency);
  EXPECT_TRUE(
      scroll_latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                 tracker()->latency_component_id(), nullptr));
  EXPECT_FALSE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker()->latency_component_id(), nullptr));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      tracker()->latency_component_id(), nullptr));
  EXPECT_EQ(3U, scroll_latency.latency_components().size());
}

TEST_F(RenderWidgetHostLatencyTrackerTest, TouchBlockingAndQueueingTime) {
  // These numbers are sensitive to where the histogram buckets are.
  int touchstart_timestamps_ms[] = {11, 25, 35};
  int touchmove_timestamps_ms[] = {1, 5, 12};
  int touchend_timestamps_ms[] = {3, 8, 12};

  for (InputEventAckState blocking :
       {INPUT_EVENT_ACK_STATE_NOT_CONSUMED, INPUT_EVENT_ACK_STATE_CONSUMED}) {
    SyntheticWebTouchEvent event;
    {
      // Touch start.
      event.PressPoint(1, 1);

      ui::LatencyInfo latency;
      latency.set_source_event_type(ui::SourceEventType::TOUCH);
      tracker()->OnInputEvent(event, &latency);

      ui::LatencyInfo fake_latency;
      fake_latency.set_trace_id(kTraceEventId);
      fake_latency.set_source_event_type(ui::SourceEventType::TOUCH);
      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[0]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[1]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[2]),
          1);

      // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
      // overwriting components.
      tracker()->ComputeInputLatencyHistograms(
          event.GetType(), tracker()->latency_component_id(), fake_latency,
          blocking);

      tracker()->OnInputEventAck(event, &latency,
                                 blocking);
    }

    {
      // Touch move.
      ui::LatencyInfo latency;
      latency.set_source_event_type(ui::SourceEventType::TOUCH);
      event.MovePoint(0, 20, 20);
      tracker()->OnInputEvent(event, &latency);

      EXPECT_TRUE(latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      EXPECT_TRUE(
          latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                   tracker()->latency_component_id(), nullptr));

      EXPECT_EQ(2U, latency.latency_components().size());

      ui::LatencyInfo fake_latency;
      fake_latency.set_trace_id(kTraceEventId);
      fake_latency.set_source_event_type(ui::SourceEventType::TOUCH);
      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchmove_timestamps_ms[0]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchmove_timestamps_ms[1]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchmove_timestamps_ms[2]),
          1);

      // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
      // overwriting components.
      tracker()->ComputeInputLatencyHistograms(
          event.GetType(), tracker()->latency_component_id(), fake_latency,
          blocking);
    }

    {
      // Touch end.
      ui::LatencyInfo latency;
      latency.set_source_event_type(ui::SourceEventType::TOUCH);
      event.ReleasePoint(0);
      tracker()->OnInputEvent(event, &latency);

      EXPECT_TRUE(latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, nullptr));
      EXPECT_TRUE(
          latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                                   tracker()->latency_component_id(), nullptr));

      EXPECT_EQ(2U, latency.latency_components().size());

      ui::LatencyInfo fake_latency;
      fake_latency.set_trace_id(kTraceEventId);
      fake_latency.set_source_event_type(ui::SourceEventType::TOUCH);
      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchend_timestamps_ms[0]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchend_timestamps_ms[1]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchend_timestamps_ms[2]),
          1);

      // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
      // overwriting components.
      tracker()->ComputeInputLatencyHistograms(
          event.GetType(), tracker()->latency_component_id(), fake_latency,
          blocking);
    }
  }

  // Touch start.
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.QueueingTime.TouchStartDefaultPrevented"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[1] - touchstart_timestamps_ms[0], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.QueueingTime.TouchStartDefaultAllowed"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[1] - touchstart_timestamps_ms[0], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.BlockingTime.TouchStartDefaultPrevented"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[2] - touchstart_timestamps_ms[1], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.BlockingTime.TouchStartDefaultAllowed"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[2] - touchstart_timestamps_ms[1], 1)));

  // Touch move.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchMoveDefaultPrevented"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[1] - touchmove_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchMoveDefaultAllowed"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[1] - touchmove_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchMoveDefaultPrevented"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[2] - touchmove_timestamps_ms[1], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchMoveDefaultAllowed"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[2] - touchmove_timestamps_ms[1], 1)));

  // Touch end.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchEndDefaultPrevented"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[1] - touchend_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchEndDefaultAllowed"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[1] - touchend_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchEndDefaultPrevented"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[2] - touchend_timestamps_ms[1], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchEndDefaultAllowed"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[2] - touchend_timestamps_ms[1], 1)));
}

TEST_F(RenderWidgetHostLatencyTrackerTest, KeyBlockingAndQueueingTime) {
  // These numbers are sensitive to where the histogram buckets are.
  int event_timestamps_ms[] = {11, 25, 35};

  for (InputEventAckState blocking :
       {INPUT_EVENT_ACK_STATE_NOT_CONSUMED, INPUT_EVENT_ACK_STATE_CONSUMED}) {
    {
      NativeWebKeyboardEvent event(blink::WebKeyboardEvent::kRawKeyDown,
                                   blink::WebInputEvent::kNoModifiers,
                                   base::TimeTicks::Now());
      ui::LatencyInfo latency_info;
      latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);
      tracker()->OnInputEvent(event, &latency_info);

      ui::LatencyInfo fake_latency;
      fake_latency.set_trace_id(kTraceEventId);
      fake_latency.set_source_event_type(ui::SourceEventType::KEY_PRESS);
      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          tracker()->latency_component_id(), 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(event_timestamps_ms[0]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(event_timestamps_ms[1]),
          1);

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(event_timestamps_ms[2]),
          1);

      // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
      // overwriting components.
      tracker()->ComputeInputLatencyHistograms(
          event.GetType(), tracker()->latency_component_id(), fake_latency,
          blocking);

      tracker()->OnInputEventAck(event, &latency_info, blocking);
    }
  }

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.QueueingTime.KeyPressDefaultPrevented"),
      ElementsAre(Bucket(event_timestamps_ms[1] - event_timestamps_ms[0], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.QueueingTime.KeyPressDefaultAllowed"),
      ElementsAre(Bucket(event_timestamps_ms[1] - event_timestamps_ms[0], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.BlockingTime.KeyPressDefaultPrevented"),
      ElementsAre(Bucket(event_timestamps_ms[2] - event_timestamps_ms[1], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.BlockingTime.KeyPressDefaultAllowed"),
      ElementsAre(Bucket(event_timestamps_ms[2] - event_timestamps_ms[1], 1)));
}

TEST_F(RenderWidgetHostLatencyTrackerTest, KeyUILatency) {
  // These numbers are sensitive to where the histogram buckets are.
  int event_timestamps_microseconds[] = {100, 185};

  NativeWebKeyboardEvent event(blink::WebKeyboardEvent::kChar,
                               blink::WebInputEvent::kNoModifiers,
                               base::TimeTicks::Now());
  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(kTraceEventId);
  latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);
  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[0]),
      1);

  // Add the BEGIN_RWH component explicitly here with a timestamp, instead of
  // calling tracker()->OnInputEvent().
  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      tracker()->latency_component_id(), 0,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[1]),
      1);

  tracker()->OnInputEventAck(event, &latency_info,
                             InputEventAckState::INPUT_EVENT_ACK_STATE_UNKNOWN);
  EXPECT_THAT(
      histogram_tester().GetAllSamples("Event.Latency.Browser.KeyPressUI"),
      ElementsAre(Bucket(
          event_timestamps_microseconds[1] - event_timestamps_microseconds[0],
          1)));
}

TEST_F(RenderWidgetHostLatencyTrackerTest, KeyAckedLatency) {
  // These numbers are sensitive to where the histogram buckets are.
  int event_timestamps_microseconds[] = {11, 24};

  NativeWebKeyboardEvent event(blink::WebKeyboardEvent::kRawKeyDown,
                               blink::WebInputEvent::kNoModifiers,
                               base::TimeTicks::Now());
  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(kTraceEventId);
  latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);

  // Add the BEGIN_RWH component explicitly here with a timestamp, instead of
  // calling tracker()->OnInputEvent().
  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      tracker()->latency_component_id(), 0,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[0]),
      1);

  // Add the ACK_RWH component explicitly here with a timestamp, instead of
  // calling tracker()->OnInputEventAck().
  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[1]),
      1);

  // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
  // overwriting components.
  tracker()->ComputeInputLatencyHistograms(
      event.GetType(), tracker()->latency_component_id(), latency_info,
      InputEventAckState::INPUT_EVENT_ACK_STATE_UNKNOWN);

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Event.Latency.Browser.KeyPressAcked"),
      ElementsAre(Bucket(
          event_timestamps_microseconds[1] - event_timestamps_microseconds[0],
          1)));
}

TEST_F(RenderWidgetHostLatencyTrackerTest, KeyEndToEndLatency) {
  // These numbers are sensitive to where the histogram buckets are.
  int event_timestamps_microseconds[] = {11, 24};

  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(kTraceEventId);
  latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);
  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0, 0,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[0]),
      1);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      tracker()->latency_component_id(), 0,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[0]),
      1);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[1]),
      1);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[1]),
      1);

  tracker()->OnGpuSwapBuffersCompleted(latency_info);

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Event.Latency.EndToEnd.KeyPress"),
      ElementsAre(Bucket(
          event_timestamps_microseconds[1] - event_timestamps_microseconds[0],
          1)));
}

// Event.Latency.(Queueing|Blocking)Time.* histograms shouldn't be reported for
// multi-finger touch.
TEST_F(RenderWidgetHostLatencyTrackerTest,
       MultiFingerTouchIgnoredForQueueingAndBlockingTimeMetrics) {
  SyntheticWebTouchEvent event;
  InputEventAckState ack_state = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  {
    // First touch start.
    ui::LatencyInfo latency;
    event.PressPoint(1, 1);
    tracker()->OnInputEvent(event, &latency);
    tracker()->OnInputEventAck(event, &latency, ack_state);
  }

  {
    // Additional touch start will be ignored for queueing and blocking time
    // metrics.
    int touchstart_timestamps_ms[] = {11, 25, 35};
    ui::LatencyInfo latency;
    event.PressPoint(1, 1);
    tracker()->OnInputEvent(event, &latency);

    ui::LatencyInfo fake_latency;
    fake_latency.set_trace_id(kTraceEventId);
    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
        tracker()->latency_component_id(), 0,
        base::TimeTicks() +
            base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[0]),
        1);

    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0, 0,
        base::TimeTicks() +
            base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[1]),
        1);

    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0,
        base::TimeTicks() +
            base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[2]),
        1);

    // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
    // overwriting components.
    tracker()->ComputeInputLatencyHistograms(event.GetType(),
                                             tracker()->latency_component_id(),
                                             fake_latency, ack_state);

    tracker()->OnInputEventAck(event, &latency, ack_state);
  }

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchStartDefaultAllowed"),
              ElementsAre());
}

// Some touch input histograms aren't reported for multi-finger touch. Other
// input modalities shouldn't be impacted by there being an active multi-finger
// touch gesture.
TEST_F(RenderWidgetHostLatencyTrackerTest, WheelDuringMultiFingerTouch) {
  SyntheticWebTouchEvent touch_event;
  InputEventAckState ack_state = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  {
    // First touch start.
    ui::LatencyInfo latency;
    latency.set_source_event_type(ui::SourceEventType::TOUCH);
    touch_event.PressPoint(1, 1);
    tracker()->OnInputEvent(touch_event, &latency);
    tracker()->OnInputEventAck(touch_event, &latency, ack_state);
  }

  {
    // Second touch start.
    ui::LatencyInfo latency;
    latency.set_source_event_type(ui::SourceEventType::TOUCH);
    touch_event.PressPoint(1, 1);
    tracker()->OnInputEvent(touch_event, &latency);
    tracker()->OnInputEventAck(touch_event, &latency, ack_state);
  }

  {
    // Wheel event.
    ui::LatencyInfo latency;
    latency.set_source_event_type(ui::SourceEventType::WHEEL);
    // These numbers are sensitive to where the histogram buckets are.
    int timestamps_ms[] = {11, 25, 35};
    auto wheel_event = SyntheticWebMouseWheelEventBuilder::Build(
        blink::WebMouseWheelEvent::kPhaseChanged);
    tracker()->OnInputEvent(touch_event, &latency);

    ui::LatencyInfo fake_latency;
    fake_latency.set_trace_id(kTraceEventId);
    fake_latency.set_source_event_type(ui::SourceEventType::TOUCH);
    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
        tracker()->latency_component_id(), 0,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(timestamps_ms[0]),
        1);

    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0, 0,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(timestamps_ms[1]),
        1);

    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(timestamps_ms[2]),
        1);

    // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
    // overwriting components.
    tracker()->ComputeInputLatencyHistograms(wheel_event.GetType(),
                                             tracker()->latency_component_id(),
                                             fake_latency, ack_state);

    tracker()->OnInputEventAck(wheel_event, &latency, ack_state);
  }

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.MouseWheelDefaultAllowed"),
              ElementsAre(Bucket(14, 1)));
}

TEST_F(RenderWidgetHostLatencyTrackerTest, ExpectedQueueingTimeAccuracy) {
  // These numbers are sensitive to where the histogram buckets are.
  base::TimeTicks event_timestamp =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(11);
  int expected_queueing_time_ms = 1;
  base::TimeDelta expected_queueing_time =
      base::TimeDelta::FromMilliseconds(expected_queueing_time_ms);

  for (float queueing_time_ms : {2, 15, 200, 400}) {
    base::TimeDelta queueing_time =
        base::TimeDelta::FromMilliseconds(queueing_time_ms);
    SyntheticWebTouchEvent event;
    // Touch start.
    event.PressPoint(1, 1);

    ui::LatencyInfo latency;
    latency.set_source_event_type(ui::SourceEventType::TOUCH);
    tracker()->OnInputEvent(event, &latency);

    ui::LatencyInfo fake_latency;
    fake_latency.set_trace_id(kTraceEventId);
    fake_latency.set_expected_queueing_time_on_dispatch(expected_queueing_time);
    fake_latency.set_source_event_type(ui::SourceEventType::TOUCH);
    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
        tracker()->latency_component_id(), 0, event_timestamp, 1);

    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0, 0,
        event_timestamp + queueing_time, 1);

    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0,
        event_timestamp + queueing_time, 1);

    // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
    // overwriting components.
    tracker()->ComputeInputLatencyHistograms(
        event.GetType(), tracker()->latency_component_id(), fake_latency,
        INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

    tracker()->OnInputEventAck(event, &latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  }

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "RendererScheduler."
                  "ExpectedQueueingTimeWhenQueueingTime_LessThan.10ms"),
              ElementsAre(Bucket(expected_queueing_time_ms, 1)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "RendererScheduler."
                  "ExpectedQueueingTimeWhenQueueingTime_LessThan.150ms"),
              ElementsAre(Bucket(expected_queueing_time_ms, 2)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "RendererScheduler."
                  "ExpectedQueueingTimeWhenQueueingTime_LessThan.300ms"),
              ElementsAre(Bucket(expected_queueing_time_ms, 3)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "RendererScheduler."
                  "ExpectedQueueingTimeWhenQueueingTime_LessThan.450ms"),
              ElementsAre(Bucket(expected_queueing_time_ms, 4)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "RendererScheduler."
                  "ExpectedQueueingTimeWhenQueueingTime_GreaterThan.10ms"),
              ElementsAre(Bucket(expected_queueing_time_ms, 3)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "RendererScheduler."
                  "ExpectedQueueingTimeWhenQueueingTime_GreaterThan.150ms"),
              ElementsAre(Bucket(expected_queueing_time_ms, 2)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "RendererScheduler."
                  "ExpectedQueueingTimeWhenQueueingTime_GreaterThan.300ms"),
              ElementsAre(Bucket(expected_queueing_time_ms, 1)));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "RendererScheduler."
                  "ExpectedQueueingTimeWhenQueueingTime_GreaterThan.450ms"),
              ElementsAre());
}

}  // namespace content
