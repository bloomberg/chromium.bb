// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

#if defined(OS_ANDROID)
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#endif

using blink::WebMouseWheelEvent;

namespace {
void GiveItSomeTime() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromMilliseconds(20));
  run_loop.Run();
}

const char kWheelEventLatchingDataURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<meta name='viewport' content='width=device-width, minimum-scale=1'>"
    "<style>"
    "body {"
    " height: 10000px;"
    "}"
    "#scrollableDiv {"
    " position: absolute;"
    " left: 50px;"
    " top: 100px;"
    " width: 200px;"
    " height: 200px;"
    " overflow: scroll;"
    " background: red;"
    "}"
    "#nestedDiv {"
    " width: 200px;"
    " height: 8000px;"
    " opacity: 0;"
    "}"
    "</style>"
    "<div id='scrollableDiv'>"
    " <div id='nestedDiv'></div>"
    "</div>"
    "<script>"
    " var scrollableDiv = document.getElementById('scrollableDiv');"
    " var scrollableDivWheelEventCounter = 0;"
    " var documentWheelEventCounter = 0;"
    " scrollableDiv.addEventListener('wheel',"
    "   function(e) { scrollableDivWheelEventCounter++;"
    "                 e.stopPropagation(); });"
    " document.scrollingElement.addEventListener('wheel',"
    "   function(e) { documentWheelEventCounter++; });"
    "</script>";

enum WheelScrollingMode {
  kWheelScrollingModeNone,
  kWheelScrollLatching,
  kAsyncWheelEvents,
};
}  // namespace

namespace content {
class WheelScrollLatchingBrowserTest : public ContentBrowserTest {
 public:
  WheelScrollLatchingBrowserTest(
      WheelScrollingMode wheel_scrolling_mode = kWheelScrollLatching)
      : wheel_scrolling_mode_(wheel_scrolling_mode),
        wheel_scroll_latching_enabled_(wheel_scrolling_mode_ !=
                                       kWheelScrollingModeNone) {
    ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
        0);

    SetFeatureList();
  }
  ~WheelScrollLatchingBrowserTest() override {}

 protected:
  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        web_contents()->GetRenderViewHost()->GetWidget());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderWidgetHostInputEventRouter* GetRouter() {
    return web_contents()->GetInputEventRouter();
  }

  RenderWidgetHostViewBase* GetRootView() {
    return static_cast<RenderWidgetHostViewBase*>(web_contents()
                                                      ->GetFrameTree()
                                                      ->root()
                                                      ->current_frame_host()
                                                      ->GetView());
  }

  void LoadURL() {
    const GURL data_url(kWheelEventLatchingDataURL);
    NavigateToURL(shell(), data_url);

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(600, 600));

    // The page is loaded in the renderer, wait for a new frame to arrive.
    while (!host->ScheduleComposite())
      GiveItSomeTime();
  }
  int ExecuteScriptAndExtractInt(const std::string& script) {
    int value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        shell(), "domAutomationController.send(" + script + ")", &value));
    return value;
  }
  double ExecuteScriptAndExtractDouble(const std::string& script) {
    double value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractDouble(
        shell(), "domAutomationController.send(" + script + ")", &value));
    return value;
  }
  std::string ExecuteScriptAndExtractString(const std::string& script) {
    std::string value = "";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        shell(), "domAutomationController.send(" + script + ")", &value));
    return value;
  }
  void SetFeatureList() {
    if (wheel_scrolling_mode_ == kAsyncWheelEvents) {
      feature_list_.InitWithFeatures({features::kTouchpadAndWheelScrollLatching,
                                      features::kAsyncWheelEvents},
                                     {});
    } else if (wheel_scrolling_mode_ == kWheelScrollLatching) {
      feature_list_.InitWithFeatures(
          {features::kTouchpadAndWheelScrollLatching},
          {features::kAsyncWheelEvents});
    } else if (wheel_scrolling_mode_ == kWheelScrollingModeNone) {
      feature_list_.InitWithFeatures({},
                                     {features::kTouchpadAndWheelScrollLatching,
                                      features::kAsyncWheelEvents});
    }
  }

  void WheelEventTargetTest();
  void WheelEventRetargetWhenTargetRemovedTest();
  void WheelScrollingRelatchWhenLatchedScrollerRemovedTest();

 private:
  base::test::ScopedFeatureList feature_list_;
  WheelScrollingMode wheel_scrolling_mode_;
  bool wheel_scroll_latching_enabled_;
};

class WheelScrollLatchingDisabledBrowserTest
    : public WheelScrollLatchingBrowserTest {
 public:
  WheelScrollLatchingDisabledBrowserTest()
      : WheelScrollLatchingBrowserTest(kWheelScrollingModeNone) {}
  ~WheelScrollLatchingDisabledBrowserTest() override {}
};

class AsyncWheelEventsBrowserTest : public WheelScrollLatchingBrowserTest {
 public:
  AsyncWheelEventsBrowserTest()
      : WheelScrollLatchingBrowserTest(kAsyncWheelEvents) {}
  ~AsyncWheelEventsBrowserTest() override {}
};

void WheelScrollLatchingBrowserTest::WheelEventTargetTest() {
  LoadURL();
  EXPECT_EQ(0, ExecuteScriptAndExtractInt("documentWheelEventCounter"));
  EXPECT_EQ(0, ExecuteScriptAndExtractInt("scrollableDivWheelEventCounter"));

  MainThreadFrameObserver frame_observer(
      shell()->web_contents()->GetRenderViewHost()->GetWidget());

  auto input_msg_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::kMouseWheel);

  float scrollable_div_top = ExecuteScriptAndExtractDouble(
      "scrollableDiv.getBoundingClientRect().top");
  float x = (ExecuteScriptAndExtractDouble(
                 "scrollableDiv.getBoundingClientRect().left") +
             ExecuteScriptAndExtractDouble(
                 "scrollableDiv.getBoundingClientRect().right")) /
            2;
  float y = 0.5 * scrollable_div_top;
  float delta_x = 0;
  float delta_y = -0.6 * scrollable_div_top;
  blink::WebMouseWheelEvent wheel_event =
      SyntheticWebMouseWheelEventBuilder::Build(x, y, x, y, delta_x, delta_y, 0,
                                                true);

  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  GetRouter()->RouteMouseWheelEvent(GetRootView(), &wheel_event,
                                    ui::LatencyInfo());

  // Runs until we get the InputMsgAck callback.
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
            input_msg_watcher->WaitForAck());

  while (ExecuteScriptAndExtractDouble("document.scrollingElement.scrollTop") <
         -delta_y) {
    frame_observer.Wait();
  }

  EXPECT_EQ(0, ExecuteScriptAndExtractDouble("scrollableDiv.scrollTop"));
  EXPECT_EQ(1, ExecuteScriptAndExtractInt("documentWheelEventCounter"));
  EXPECT_EQ(0, ExecuteScriptAndExtractInt("scrollableDivWheelEventCounter"));

  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseChanged;
  GetRouter()->RouteMouseWheelEvent(GetRootView(), &wheel_event,
                                    ui::LatencyInfo());

  if (wheel_scrolling_mode_ != kAsyncWheelEvents) {
    // Runs until we get the InputMsgAck callback.
    EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
              input_msg_watcher->WaitForAck());
  }

  if (wheel_scroll_latching_enabled_) {
    while (ExecuteScriptAndExtractDouble(
               "document.scrollingElement.scrollTop") < -2 * delta_y) {
      frame_observer.Wait();
    }

    EXPECT_EQ(0, ExecuteScriptAndExtractDouble("scrollableDiv.scrollTop"));
    EXPECT_EQ(2, ExecuteScriptAndExtractInt("documentWheelEventCounter"));
    EXPECT_EQ(0, ExecuteScriptAndExtractInt("scrollableDivWheelEventCounter"));
    } else {  // !wheel_scroll_latching_enabled_
      while (ExecuteScriptAndExtractDouble("scrollableDiv.scrollTop") <
             -delta_y)
        frame_observer.Wait();

      EXPECT_EQ(1, ExecuteScriptAndExtractInt("documentWheelEventCounter"));
      EXPECT_EQ(1,
                ExecuteScriptAndExtractInt("scrollableDivWheelEventCounter"));
    }
  }
// Start scrolling by mouse wheel on the document: the wheel event will be sent
// to the document's scrolling element, the scrollable div will be under the
// cursor after applying the scrolling. Continue scrolling by mouse wheel, since
// wheel scroll latching is enabled the wheel event will be still sent to the
// document's scrolling element and the document's scrolling element will
// continue scrolling.
IN_PROC_BROWSER_TEST_F(WheelScrollLatchingBrowserTest, WheelEventTarget) {
  WheelEventTargetTest();
}
IN_PROC_BROWSER_TEST_F(AsyncWheelEventsBrowserTest, WheelEventTarget) {
  WheelEventTargetTest();
}

// Start scrolling by mouse wheel on the document: the wheel event will be sent
// to the document's scrolling element, the scrollable div will be under the
// cursor after applying the scrolloffsets. Continue scrolling by mouse wheel,
// since wheel scroll latching is disabled the wheel event will be still sent to
// the scrollable div which is currently under the cursor. The div will start
// scrolling.
IN_PROC_BROWSER_TEST_F(WheelScrollLatchingDisabledBrowserTest,
                       WheelEventTarget) {
  WheelEventTargetTest();
}

// Tests that wheel events are retargeted if their target gets deleted in the
// middle of scrolling.
void WheelScrollLatchingBrowserTest::WheelEventRetargetWhenTargetRemovedTest() {
  // The test is valid only when wheel scroll latching is enabled.
  if (!wheel_scroll_latching_enabled_)
    return;

  LoadURL();
  EXPECT_EQ(0, ExecuteScriptAndExtractInt("documentWheelEventCounter"));
  EXPECT_EQ(0, ExecuteScriptAndExtractInt("scrollableDivWheelEventCounter"));

  auto update_msg_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::kGestureScrollUpdate);

  float scrollable_div_top = ExecuteScriptAndExtractDouble(
      "scrollableDiv.getBoundingClientRect().top");
  float x = (ExecuteScriptAndExtractDouble(
                 "scrollableDiv.getBoundingClientRect().left") +
             ExecuteScriptAndExtractDouble(
                 "scrollableDiv.getBoundingClientRect().right")) /
            2;
  float y = 1.1 * scrollable_div_top;
  float delta_x = 0;
  float delta_y = -0.6 * scrollable_div_top;
  blink::WebMouseWheelEvent wheel_event =
      SyntheticWebMouseWheelEventBuilder::Build(x, y, x, y, delta_x, delta_y, 0,
                                                true);
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  GetRouter()->RouteMouseWheelEvent(GetRootView(), &wheel_event,
                                    ui::LatencyInfo());

  // Runs until we get the UpdateMsgAck callback.
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, update_msg_watcher->WaitForAck());

  EXPECT_EQ(
      0, ExecuteScriptAndExtractDouble("document.scrollingElement.scrollTop"));
  EXPECT_EQ(0, ExecuteScriptAndExtractInt("documentWheelEventCounter"));
  EXPECT_EQ(1, ExecuteScriptAndExtractInt("scrollableDivWheelEventCounter"));

  // Remove the scrollableDiv which is the current target for wheel events.
  EXPECT_TRUE(ExecuteScript(
      shell(), "scrollableDiv.parentNode.removeChild(scrollableDiv)"));

  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseChanged;
  GetRouter()->RouteMouseWheelEvent(GetRootView(), &wheel_event,
                                    ui::LatencyInfo());

  // Runs until we get the UpdateMsgAck callbacks.
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, update_msg_watcher->WaitForAck());

  // Wait for the document event listenr to handle the second wheel event.
  while (ExecuteScriptAndExtractInt("documentWheelEventCounter") != 1) {
    GiveItSomeTime();
  }

  EXPECT_EQ(1, ExecuteScriptAndExtractInt("scrollableDivWheelEventCounter"));
}
IN_PROC_BROWSER_TEST_F(WheelScrollLatchingBrowserTest,
                       WheelEventRetargetWhenTargetRemoved) {
  WheelEventRetargetWhenTargetRemovedTest();
}
IN_PROC_BROWSER_TEST_F(AsyncWheelEventsBrowserTest,
                       WheelEventRetargetWhenTargetRemoved) {
  WheelEventRetargetWhenTargetRemovedTest();
}

// crbug.com/777258 Flaky on Android.
#if defined(OS_ANDROID)
#define MAYBE_WheelScrollingRelatchWhenLatchedScrollerRemoved \
  DISABLED_WheelScrollingRelatchWhenLatchedScrollerRemoved
#else
#define MAYBE_WheelScrollingRelatchWhenLatchedScrollerRemoved \
  WheelScrollingRelatchWhenLatchedScrollerRemoved
#endif
void WheelScrollLatchingBrowserTest::
    WheelScrollingRelatchWhenLatchedScrollerRemovedTest() {
  // The test is valid only when wheel scroll latching is enabled.
  if (!wheel_scroll_latching_enabled_)
    return;

  LoadURL();
  EXPECT_EQ(
      ExecuteScriptAndExtractDouble("document.scrollingElement.scrollTop"), 0);
  EXPECT_EQ(ExecuteScriptAndExtractDouble("scrollableDiv.scrollTop"), 0);
  float x = (ExecuteScriptAndExtractDouble(
                 "scrollableDiv.getBoundingClientRect().left") +
             ExecuteScriptAndExtractDouble(
                 "scrollableDiv.getBoundingClientRect().right")) /
            2;
  float y = (ExecuteScriptAndExtractDouble(
                 "scrollableDiv.getBoundingClientRect().top") +
             ExecuteScriptAndExtractDouble(
                 "scrollableDiv.getBoundingClientRect().bottom")) /
            2;
#if defined(OS_CHROMEOS)
  bool precise = true;
#else
  bool precise = false;
#endif
  // Send a GSB event to start scrolling the scrollableDiv.
  blink::WebGestureEvent gesture_scroll_begin(
      blink::WebGestureEvent::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  gesture_scroll_begin.source_device = blink::kWebGestureDeviceTouchpad;
  gesture_scroll_begin.data.scroll_begin.delta_hint_units =
      precise ? blink::WebGestureEvent::ScrollUnits::kPrecisePixels
              : blink::WebGestureEvent::ScrollUnits::kPixels;
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0.f;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = -20.f;
  gesture_scroll_begin.x = x;
  gesture_scroll_begin.y = y;
  gesture_scroll_begin.global_x = x;
  gesture_scroll_begin.global_y = y;
  GetRootView()->ProcessGestureEvent(gesture_scroll_begin, ui::LatencyInfo());

  // Send the first GSU event.
  blink::WebGestureEvent gesture_scroll_update(gesture_scroll_begin);
  gesture_scroll_update.SetType(blink::WebGestureEvent::kGestureScrollUpdate);
  gesture_scroll_update.data.scroll_update.delta_units =
      precise ? blink::WebGestureEvent::ScrollUnits::kPrecisePixels
              : blink::WebGestureEvent::ScrollUnits::kPixels;
  gesture_scroll_update.data.scroll_update.delta_x = 0.f;
  gesture_scroll_update.data.scroll_update.delta_y = -20.f;
  GetRootView()->ProcessGestureEvent(gesture_scroll_update, ui::LatencyInfo());

  // Wait for the scrollableDiv to scroll.
  while (ExecuteScriptAndExtractDouble("scrollableDiv.scrollTop") < 20)
    GiveItSomeTime();

  // Remove the scrollableDiv which is the current scroller and send the second
  // GSU.
  EXPECT_TRUE(ExecuteScript(
      shell(), "scrollableDiv.parentNode.removeChild(scrollableDiv)"));
  GiveItSomeTime();
  GetRootView()->ProcessGestureEvent(gesture_scroll_update, ui::LatencyInfo());
  while (ExecuteScriptAndExtractDouble("document.scrollingElement.scrollTop") <
         20) {
    GiveItSomeTime();
  }
}
IN_PROC_BROWSER_TEST_F(WheelScrollLatchingBrowserTest,
                       MAYBE_WheelScrollingRelatchWhenLatchedScrollerRemoved) {
  WheelScrollingRelatchWhenLatchedScrollerRemovedTest();
}
IN_PROC_BROWSER_TEST_F(AsyncWheelEventsBrowserTest,
                       MAYBE_WheelScrollingRelatchWhenLatchedScrollerRemoved) {
  WheelScrollingRelatchWhenLatchedScrollerRemovedTest();
}

}  // namespace content
