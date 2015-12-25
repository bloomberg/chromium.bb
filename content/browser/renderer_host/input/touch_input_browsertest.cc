// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event_switches.h"
#include "ui/events/latency_info.h"

using blink::WebInputEvent;

namespace {

void GiveItSomeTime() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromMilliseconds(10));
  run_loop.Run();
}

const char kTouchEventDataURL[] =
    "data:text/html;charset=utf-8,"
#if defined(OS_ANDROID)
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "</head>"
#endif
    "<body onload='setup();'>"
    "<div id='first'></div><div id='second'></div><div id='third'></div>"
    "<style>"
    "  #first {"
    "    position: absolute;"
    "    width: 100px;"
    "    height: 100px;"
    "    top: 0px;"
    "    left: 0px;"
    "    background-color: green;"
    "    -webkit-transform: translate3d(0, 0, 0);"
    "  }"
    "  #second {"
    "    position: absolute;"
    "    width: 100px;"
    "    height: 100px;"
    "    top: 0px;"
    "    left: 110px;"
    "    background-color: blue;"
    "    -webkit-transform: translate3d(0, 0, 0);"
    "  }"
    "  #third {"
    "    position: absolute;"
    "    width: 100px;"
    "    height: 100px;"
    "    top: 110px;"
    "    left: 0px;"
    "    background-color: yellow;"
    "    -webkit-transform: translate3d(0, 0, 0);"
    "  }"
    "</style>"
    "<script>"
    "  function setup() {"
    "    second.ontouchstart = function() {};"
    "    third.ontouchstart = function(e) {"
    "      e.preventDefault();"
    "    };"
    "  }"
    "</script>";

}  // namespace

namespace content {

class InputEventMessageFilter : public BrowserMessageFilter {
 public:
  InputEventMessageFilter()
      : BrowserMessageFilter(InputMsgStart),
        type_(WebInputEvent::Undefined),
        state_(INPUT_EVENT_ACK_STATE_UNKNOWN) {}

  void WaitForAck(WebInputEvent::Type type) {
    base::RunLoop run_loop;
    base::AutoReset<base::Closure> reset_quit(&quit_, run_loop.QuitClosure());
    base::AutoReset<WebInputEvent::Type> reset_type(&type_, type);
    run_loop.Run();
  }

  InputEventAckState last_ack_state() const { return state_; }

 protected:
  ~InputEventMessageFilter() override {}

 private:
  void ReceivedEventAck(WebInputEvent::Type type, InputEventAckState state) {
    if (type_ == type) {
      state_ = state;
      quit_.Run();
    }
  }

  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    if (message.type() == InputHostMsg_HandleInputEvent_ACK::ID) {
      InputHostMsg_HandleInputEvent_ACK::Param params;
      InputHostMsg_HandleInputEvent_ACK::Read(&message, &params);
      WebInputEvent::Type type = base::get<0>(params).type;
      InputEventAckState ack = base::get<0>(params).state;
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&InputEventMessageFilter::ReceivedEventAck,
                     this, type, ack));
    }
    return false;
  }

  base::Closure quit_;
  WebInputEvent::Type type_;
  InputEventAckState state_;

  DISALLOW_COPY_AND_ASSIGN(InputEventMessageFilter);
};

class TouchInputBrowserTest : public ContentBrowserTest {
 public:
  TouchInputBrowserTest() {}
  ~TouchInputBrowserTest() override {}

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  InputEventMessageFilter* filter() { return filter_.get(); }

 protected:
  void LoadURLAndAddFilter() {
    const GURL data_url(kTouchEventDataURL);
    NavigateToURL(shell(), data_url);

    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
        web_contents->GetRenderViewHost()->GetWidget());
    host->GetView()->SetSize(gfx::Size(400, 400));

    // The page is loaded in the renderer, wait for a new frame to arrive.
    while (!host->ScheduleComposite())
      GiveItSomeTime();

    filter_ = new InputEventMessageFilter();
    host->GetProcess()->AddFilter(filter_.get());
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitchASCII(switches::kTouchEvents,
                           switches::kTouchEventsEnabled);
  }

  scoped_refptr<InputEventMessageFilter> filter_;
};

#if defined(OS_MACOSX)
// TODO(ccameron): Failing on mac: crbug.com/346363
#define MAYBE_TouchNoHandler DISABLED_TouchNoHandler
#else
#define MAYBE_TouchNoHandler TouchNoHandler
#endif
IN_PROC_BROWSER_TEST_F(TouchInputBrowserTest, MAYBE_TouchNoHandler) {
  LoadURLAndAddFilter();
  SyntheticWebTouchEvent touch;

  // A press on |first| should be acked with NO_CONSUMER_EXISTS since there is
  // no touch-handler on it.
  touch.PressPoint(25, 25);
  GetWidgetHost()->ForwardTouchEventWithLatencyInfo(touch, ui::LatencyInfo());
  filter()->WaitForAck(WebInputEvent::TouchStart);

  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
            filter()->last_ack_state());

  // If a touch-press is acked with NO_CONSUMER_EXISTS, then subsequent
  // touch-points don't need to be dispatched until the touch point is released.
  touch.ReleasePoint(0);
  GetWidgetHost()->ForwardTouchEventWithLatencyInfo(touch, ui::LatencyInfo());
  touch.ResetPoints();
}

#if defined(OS_CHROMEOS)
// crbug.com/514456
#define MAYBE_TouchHandlerNoConsume DISABLED_TouchHandlerNoConsume
#else
#define MAYBE_TouchHandlerNoConsume TouchHandlerNoConsume
#endif
IN_PROC_BROWSER_TEST_F(TouchInputBrowserTest, MAYBE_TouchHandlerNoConsume) {
  LoadURLAndAddFilter();
  SyntheticWebTouchEvent touch;

  // Press on |second| should be acked with NOT_CONSUMED since there is a
  // touch-handler on |second|, but it doesn't consume the event.
  touch.PressPoint(125, 25);
  GetWidgetHost()->ForwardTouchEventWithLatencyInfo(touch, ui::LatencyInfo());
  filter()->WaitForAck(WebInputEvent::TouchStart);
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, filter()->last_ack_state());

  touch.ReleasePoint(0);
  GetWidgetHost()->ForwardTouchEventWithLatencyInfo(touch, ui::LatencyInfo());
  filter()->WaitForAck(WebInputEvent::TouchEnd);
  touch.ResetPoints();
}

#if defined(OS_CHROMEOS)
// crbug.com/514456
#define MAYBE_TouchHandlerConsume DISABLED_TouchHandlerConsume
#else
#define MAYBE_TouchHandlerConsume TouchHandlerConsume
#endif
IN_PROC_BROWSER_TEST_F(TouchInputBrowserTest, MAYBE_TouchHandlerConsume) {
  LoadURLAndAddFilter();
  SyntheticWebTouchEvent touch;

  // Press on |third| should be acked with CONSUMED since the touch-handler on
  // |third| consimes the event.
  touch.PressPoint(25, 125);
  GetWidgetHost()->ForwardTouchEventWithLatencyInfo(touch, ui::LatencyInfo());
  filter()->WaitForAck(WebInputEvent::TouchStart);
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, filter()->last_ack_state());

  touch.ReleasePoint(0);
  GetWidgetHost()->ForwardTouchEventWithLatencyInfo(touch, ui::LatencyInfo());
  filter()->WaitForAck(WebInputEvent::TouchEnd);
}

#if defined(OS_CHROMEOS)
// crbug.com/514456
#define MAYBE_MultiPointTouchPress DISABLED_MultiPointTouchPress
#elif defined(OS_MACOSX)
// TODO(ccameron): Failing on mac: crbug.com/346363
#define MAYBE_MultiPointTouchPress DISABLED_MultiPointTouchPress
#else
#define MAYBE_MultiPointTouchPress MultiPointTouchPress
#endif
IN_PROC_BROWSER_TEST_F(TouchInputBrowserTest, MAYBE_MultiPointTouchPress) {
  LoadURLAndAddFilter();
  SyntheticWebTouchEvent touch;

  // Press on |first|, which sould be acked with NO_CONSUMER_EXISTS. Then press
  // on |third|. That point should be acked with CONSUMED.
  touch.PressPoint(25, 25);
  GetWidgetHost()->ForwardTouchEventWithLatencyInfo(touch, ui::LatencyInfo());
  filter()->WaitForAck(WebInputEvent::TouchStart);
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
            filter()->last_ack_state());

  touch.PressPoint(25, 125);
  GetWidgetHost()->ForwardTouchEventWithLatencyInfo(touch, ui::LatencyInfo());
  filter()->WaitForAck(WebInputEvent::TouchStart);
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, filter()->last_ack_state());
}

}  // namespace content
