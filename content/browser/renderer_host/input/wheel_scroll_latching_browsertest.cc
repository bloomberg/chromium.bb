// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromMilliseconds(10));
  run_loop.Run();
}

const char kWheelEventLatchingDataURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<style>"
    "body {"
    " height: 10000px;"
    "}"
    "#scrollableDiv {"
    " position: absolute;"
    " left: 100px;"
    " top: 200px;"
    " width: 400px;"
    " height: 800px;"
    " overflow: scroll;"
    " background: red;"
    "}"
    "#nestedDiv {"
    " width: 400px;"
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
}  // namespace

namespace content {
class WheelScrollLatchingBrowserTest : public ContentBrowserTest {
 public:
  WheelScrollLatchingBrowserTest(bool wheel_scroll_latching_enabled = true)
      : wheel_scroll_latching_enabled_(wheel_scroll_latching_enabled) {
    ui::GestureConfiguration::GetInstance()->set_scroll_debounce_interval_in_ms(
        0);

    if (wheel_scroll_latching_enabled_)
      EnableWheelScrollLatching();
    else
      DisableWheelScrollLatching();
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

  float GetPageScaleFactor() {
    return GetWidgetHost()->last_frame_metadata().page_scale_factor;
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
  void EnableWheelScrollLatching() {
    feature_list_.InitFromCommandLine(
        features::kTouchpadAndWheelScrollLatching.name, "");
  }
  void DisableWheelScrollLatching() {
    feature_list_.InitFromCommandLine(
        "", features::kTouchpadAndWheelScrollLatching.name);
  }

  void WheelEventTargetTest() {
    LoadURL();
    EXPECT_EQ(0, ExecuteScriptAndExtractInt("documentWheelEventCounter"));
    EXPECT_EQ(0, ExecuteScriptAndExtractInt("scrollableDivWheelEventCounter"));

    FrameWatcher frame_watcher(shell()->web_contents());
    scoped_refptr<InputMsgWatcher> input_msg_watcher(new InputMsgWatcher(
        GetWidgetHost(), blink::WebInputEvent::kMouseWheel));
    float scale_factor = GetPageScaleFactor();

    float scrollable_div_top =
        ExecuteScriptAndExtractInt("scrollableDiv.getBoundingClientRect().top");
    float x = scale_factor *
              (ExecuteScriptAndExtractInt(
                   "scrollableDiv.getBoundingClientRect().left") +
               ExecuteScriptAndExtractInt(
                   "scrollableDiv.getBoundingClientRect().right")) /
              2;
    float y = scale_factor * 0.5 * scrollable_div_top;
    float delta_x = 0;
    float delta_y = -0.5 * scrollable_div_top;
    blink::WebMouseWheelEvent wheel_event =
        SyntheticWebMouseWheelEventBuilder::Build(x, y, x, y, delta_x, delta_y,
                                                  0, true);

    wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
    GetRouter()->RouteMouseWheelEvent(GetRootView(), &wheel_event,
                                      ui::LatencyInfo());

    // Runs until we get the InputMsgAck callback.
    EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
              input_msg_watcher->WaitForAck());

    while (ExecuteScriptAndExtractInt("document.scrollingElement.scrollTop") <
           -delta_y) {
      frame_watcher.WaitFrames(1);
    }

    EXPECT_EQ(0, ExecuteScriptAndExtractInt("scrollableDiv.scrollTop"));
    EXPECT_EQ(1, ExecuteScriptAndExtractInt("documentWheelEventCounter"));
    EXPECT_EQ(0, ExecuteScriptAndExtractInt("scrollableDivWheelEventCounter"));

    wheel_event.phase = blink::WebMouseWheelEvent::kPhaseChanged;
    GetRouter()->RouteMouseWheelEvent(GetRootView(), &wheel_event,
                                      ui::LatencyInfo());

    // Runs until we get the InputMsgAck callback.
    EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
              input_msg_watcher->WaitForAck());

    if (wheel_scroll_latching_enabled_) {
      while (ExecuteScriptAndExtractInt("document.scrollingElement.scrollTop") <
             -2 * delta_y) {
        frame_watcher.WaitFrames(1);
      }

      EXPECT_EQ(0, ExecuteScriptAndExtractInt("scrollableDiv.scrollTop"));
      EXPECT_EQ(2, ExecuteScriptAndExtractInt("documentWheelEventCounter"));
      EXPECT_EQ(0,
                ExecuteScriptAndExtractInt("scrollableDivWheelEventCounter"));
    } else {  // !wheel_scroll_latching_enabled_
      while (ExecuteScriptAndExtractInt("scrollableDiv.scrollTop") < -delta_y)
        frame_watcher.WaitFrames(1);

      EXPECT_EQ(1, ExecuteScriptAndExtractInt("documentWheelEventCounter"));
      EXPECT_EQ(1,
                ExecuteScriptAndExtractInt("scrollableDivWheelEventCounter"));
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  bool wheel_scroll_latching_enabled_;
};

class WheelScrollLatchingDisabledBrowserTest
    : public WheelScrollLatchingBrowserTest {
 public:
  WheelScrollLatchingDisabledBrowserTest()
      : WheelScrollLatchingBrowserTest(false) {}
  ~WheelScrollLatchingDisabledBrowserTest() override {}
};

// Start scrolling by mouse wheel on the document: the wheel event will be sent
// to the document's scrolling element, the scrollable div will be under the
// cursor after applying the scrolling. Continue scrolling by mouse wheel, since
// wheel scroll latching is enabled the wheel event will be still sent to the
// document's scrolling element and the document's scrolling element will
// continue scrolling.
IN_PROC_BROWSER_TEST_F(WheelScrollLatchingBrowserTest, WheelEventTarget) {
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

}  // namespace content