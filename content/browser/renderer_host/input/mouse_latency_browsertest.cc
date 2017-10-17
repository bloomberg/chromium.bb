// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_smooth_move_gesture.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kDataURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<title>Mouse event trace events reported.</title>"
    "<script src=\"../../resources/testharness.js\"></script>"
    "<script src=\"../../resources/testharnessreport.js\"></script>"
    "<script>"
    "  document.addEventListener('mousemove', () => {"
    "    var end = performance.now() + 20;"
    "    while(performance.now() < end);"
    "  });"
    "</script>"
    "<style>"
    "body {"
    "  height:3000px;"
    // Prevent text selection logic from triggering, as it makes the test flaky.
    "  user-select: none;"
    "}"
    "</style>"
    "</head>"
    "<body>"
    "</body>"
    "</html>";

}  // namespace

namespace content {

class MouseLatencyBrowserTest : public ContentBrowserTest {
 public:
  MouseLatencyBrowserTest() : loop_(base::MessageLoop::TYPE_UI) {}
  ~MouseLatencyBrowserTest() override {}

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    runner_->Quit();
  }

  void OnTraceDataCollected(
      std::unique_ptr<const base::DictionaryValue> metadata,
      base::RefCountedString* trace_data_string) {
    std::unique_ptr<base::Value> trace_data =
        base::JSONReader::Read(trace_data_string->data());
    ASSERT_TRUE(trace_data);
    trace_data_ = trace_data->Clone();
    runner_->Quit();
  }

 protected:
  void LoadURL() {
    const GURL data_url(kDataURL);
    NavigateToURL(shell(), data_url);

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));
  }

  // Generate mouse events for a synthetic click at |point|.
  void DoSyncClick(const gfx::PointF& position) {
    SyntheticTapGestureParams params;
    params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
    params.position = position;
    params.duration_ms = 100;
    std::unique_ptr<SyntheticTapGesture> gesture(
        new SyntheticTapGesture(params));

    GetWidgetHost()->QueueSyntheticGesture(
        std::move(gesture),
        base::BindOnce(&MouseLatencyBrowserTest::OnSyntheticGestureCompleted,
                       base::Unretained(this)));

    // Runs until we get the OnSyntheticGestureCompleted callback
    runner_ = base::MakeUnique<base::RunLoop>();
    runner_->Run();
  }

  // Generate mouse events drag from |position|.
  void DoSyncCoalescedMoves(const gfx::PointF position,
                            const gfx::Vector2dF& delta1,
                            const gfx::Vector2dF& delta2) {
    SyntheticSmoothMoveGestureParams params;
    params.input_type = SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT;
    params.start_point.SetPoint(position.x(), position.y());
    params.distances.push_back(delta1);
    params.distances.push_back(delta2);

    std::unique_ptr<SyntheticSmoothMoveGesture> gesture(
        new SyntheticSmoothMoveGesture(params));

    GetWidgetHost()->QueueSyntheticGesture(
        std::move(gesture),
        base::BindOnce(&MouseLatencyBrowserTest::OnSyntheticGestureCompleted,
                       base::Unretained(this)));

    // Runs until we get the OnSyntheticGestureCompleted callback
    runner_ = base::MakeUnique<base::RunLoop>();
    runner_->Run();
  }

  void StartTracing() {
    base::trace_event::TraceConfig trace_config(
        "{"
        "\"enable_argument_filter\":false,"
        "\"enable_systrace\":false,"
        "\"included_categories\":["
        "\"latencyInfo\""
        "],"
        "\"record_mode\":\"record-until-full\""
        "}");

    base::RunLoop run_loop;
    ASSERT_TRUE(TracingController::GetInstance()->StartTracing(
        trace_config, run_loop.QuitClosure()));
    run_loop.Run();
  }

  const base::Value& StopTracing() {
    bool success = TracingController::GetInstance()->StopTracing(
        TracingController::CreateStringEndpoint(
            base::Bind(&MouseLatencyBrowserTest::OnTraceDataCollected,
                       base::Unretained(this))));
    EXPECT_TRUE(success);

    // Runs until we get the OnTraceDataCollected callback, which populates
    // trace_data_;
    runner_ = base::MakeUnique<base::RunLoop>();
    runner_->Run();
    return trace_data_;
  }

 private:
  base::MessageLoop loop_;
  std::unique_ptr<base::RunLoop> runner_;
  base::Value trace_data_;

  DISALLOW_COPY_AND_ASSIGN(MouseLatencyBrowserTest);
};

// Ensures that LatencyInfo async slices are reported correctly for MouseUp and
// MouseDown events in the case where no swap is generated.
// Disabled on Android because we don't support synthetic mouse input on
// Android (crbug.com/723618).
#if defined(OS_ANDROID)
#define MAYBE_MouseDownAndUpRecordedWithoutSwap \
  DISABLED_MouseDownAndUpRecordedWithoutSwap
#else
#define MAYBE_MouseDownAndUpRecordedWithoutSwap \
  MouseDownAndUpRecordedWithoutSwap
#endif
IN_PROC_BROWSER_TEST_F(MouseLatencyBrowserTest,
                       MAYBE_MouseDownAndUpRecordedWithoutSwap) {
  LoadURL();

  StartTracing();
  DoSyncClick(gfx::PointF(100, 100));
  const base::Value& trace_data = StopTracing();

  const base::DictionaryValue* trace_data_dict;
  trace_data.GetAsDictionary(&trace_data_dict);
  ASSERT_TRUE(trace_data.GetAsDictionary(&trace_data_dict));

  const base::ListValue* traceEvents;
  ASSERT_TRUE(trace_data_dict->GetList("traceEvents", &traceEvents));

  std::vector<std::string> trace_event_names;

  for (size_t i = 0; i < traceEvents->GetSize(); ++i) {
    const base::DictionaryValue* traceEvent;
    ASSERT_TRUE(traceEvents->GetDictionary(i, &traceEvent));

    std::string name;
    ASSERT_TRUE(traceEvent->GetString("name", &name));

    if (name != "InputLatency::MouseUp" && name != "InputLatency::MouseDown")
      continue;
    trace_event_names.push_back(name);
  }

  // We see two events per async slice, a begin and an end.
  EXPECT_THAT(trace_event_names,
              testing::UnorderedElementsAre(
                  "InputLatency::MouseDown", "InputLatency::MouseDown",
                  "InputLatency::MouseUp", "InputLatency::MouseUp"));
}

// Ensures that LatencyInfo async slices are reported correctly for MouseMove
// events in the case where events are coalesced. (crbug.com/771165).
// Disabled on Android because we don't support synthetic mouse input on Android
// (crbug.com/723618).
#if defined(OS_ANDROID)
#define MAYBE_CoalescedMouseMovesCorrectlyTerminated \
  DISABLED_CoalescedMouseMovesCorrectlyTerminated
#else
#define MAYBE_CoalescedMouseMovesCorrectlyTerminated \
  CoalescedMouseMovesCorrectlyTerminated
#endif
IN_PROC_BROWSER_TEST_F(MouseLatencyBrowserTest,
                       MAYBE_CoalescedMouseMovesCorrectlyTerminated) {
  LoadURL();

  StartTracing();
  DoSyncCoalescedMoves(gfx::PointF(100, 100), gfx::Vector2dF(150, 150),
                       gfx::Vector2dF(250, 250));
  const base::Value& trace_data = StopTracing();

  const base::DictionaryValue* trace_data_dict;
  trace_data.GetAsDictionary(&trace_data_dict);
  ASSERT_TRUE(trace_data.GetAsDictionary(&trace_data_dict));

  const base::ListValue* traceEvents;
  ASSERT_TRUE(trace_data_dict->GetList("traceEvents", &traceEvents));

  std::map<std::string, int> trace_ids;

  for (size_t i = 0; i < traceEvents->GetSize(); ++i) {
    const base::DictionaryValue* traceEvent;
    ASSERT_TRUE(traceEvents->GetDictionary(i, &traceEvent));

    std::string name;
    ASSERT_TRUE(traceEvent->GetString("name", &name));

    if (name != "InputLatency::MouseMove")
      continue;

    std::string id;
    if (traceEvent->GetString("id", &id))
      ++trace_ids[id];
  }

  for (auto i : trace_ids) {
    // Each trace id should show up once for the begin, and once for the end.
    EXPECT_EQ(2, i.second);
  }
}

}  // namespace content
