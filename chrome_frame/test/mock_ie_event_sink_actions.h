// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_ACTIONS_H_
#define CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_ACTIONS_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/platform_thread.h"
#include "base/scoped_bstr_win.h"
#include "base/time.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/chrome_frame_ui_test_utils.h"
#include "chrome_frame/test/ie_event_sink.h"
#include "chrome_frame/test/mock_ie_event_sink_test.h"
#include "chrome_frame/test/simulate_input.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_frame_test {

MATCHER_P(UrlPathEq, url, "equals the path and query portion of the url") {
  return arg == chrome_frame_test::GetPathAndQueryFromUrl(url);
}

ACTION_P3(DelayCloseBrowserMock, loop, delay, mock) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::CloseWebBrowser), delay);
}

ACTION_P(SetFocusToRenderer, mock) {
  simulate_input::SetKeyboardFocusToWindow(
      mock->event_sink()->GetRendererWindow());
}

ACTION_P2(Navigate, mock, navigate_url) {
  mock->event_sink()->Navigate(navigate_url);
}

ACTION_P2(WatchWindow, mock, window_class) {
  mock->WatchWindow(window_class);
}

ACTION_P(StopWindowWatching, mock) {
  mock->StopWatching();
}

ACTION_P(ExpectRendererHasFocus, mock) {
  mock->event_sink()->ExpectRendererWindowHasFocus();
}

ACTION_P2(ConnectDocPropNotifySink, mock, sink) {
  ScopedComPtr<IDispatch> document;
  mock->event_sink()->web_browser2()->get_Document(document.Receive());
  EXPECT_TRUE(document != NULL);  // NOLINT
  if (document) {
    sink->Attach(document);
  }
}

ACTION_P(DisconnectDocPropNotifySink, sink) {
  sink->Detach();
}

ACTION_P3(DelayNavigateToCurrentUrl, mock, loop, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(&NavigateToCurrentUrl,
      mock), delay);
}

ACTION_P2(ExpectDocumentReadystate, mock, ready_state) {
  mock->ExpectDocumentReadystate(ready_state);
}

ACTION_P8(DelayExecCommand, mock, loop, delay, cmd_group_guid, cmd_id,
          cmd_exec_opt, in_args, out_args) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::Exec, cmd_group_guid, cmd_id, cmd_exec_opt, in_args,
          out_args), delay);
}

ACTION_P3(DelayRefresh, mock, loop, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::Refresh), delay);
}

ACTION_P6(DelaySendMouseClick, mock, loop, delay, x, y, button) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::SendMouseClick, x, y, button), delay);
}

ACTION_P5(DelaySendDoubleClick, mock, loop, delay, x, y) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::SendMouseClick, x, y, simulate_input::LEFT), delay);
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::SendMouseClick, x, y, simulate_input::LEFT), delay + 100);
}

ACTION_P4(DelaySendChar, loop, delay, c, mod) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
      simulate_input::SendCharA, c, mod), delay);
}

ACTION_P3(DelaySendString, loop, delay, str) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
      simulate_input::SendStringW, str), delay);
}

ACTION_P5(SendExtendedKeysEnter, loop, delay, c, repeat, mod) {
  chrome_frame_test::DelaySendExtendedKeysEnter(loop, delay, c, repeat, mod);
}

namespace {

void DoCloseWindowNow(HWND hwnd) {
  ::PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
}

}  // namespace

ACTION(DoCloseWindow) {
  DoCloseWindowNow(arg0);
}

ACTION_P(DelayDoCloseWindow, delay) {
  DCHECK(MessageLoop::current());
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      NewRunnableFunction(DoCloseWindowNow, arg0),
      delay);
}

ACTION_P3(DelayGoBack, mock, loop, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::GoBack), delay);
}

ACTION_P3(DelayGoForward, mock, loop, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::GoForward), delay);
}

ACTION_P(CloseBrowserMock, mock) {
  mock->event_sink()->CloseWebBrowser();
}

ACTION_P(VerifyAddressBarUrl, mock) {
  mock->event_sink()->ExpectAddressBarUrl(std::wstring(arg1));
}

ACTION_P3(VerifyPageLoad, mock, in_cf, url) {
  EXPECT_TRUE(static_cast<bool>(in_cf) == mock->event_sink()->IsCFRendering());
  mock->event_sink()->ExpectAddressBarUrl(url);
}

ACTION_P4(DelaySendScanCode, loop, delay, c, mod) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
        simulate_input::SendScanCode, c, mod), delay);
}


ACTION(KillChromeFrameProcesses) {
  KillAllNamedProcessesWithArgument(
      UTF8ToWide(chrome_frame_test::kChromeImageName),
      UTF8ToWide(switches::kChromeFrame));
}

// This function selects the address bar via the Alt+d shortcut. This is done
// via a delayed task which executes after the delay which is passed in.
// The subsequent operations like typing in the actual url and then hitting
// enter to force the browser to navigate to it each execute as delayed tasks
// timed at the delay passed in. The recommended value for delay is 2000 ms
// to account for the time taken for the accelerator keys to be reflected back
// from Chrome.
ACTION_P3(TypeUrlInAddressBar, loop, url, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
      simulate_input::SendCharA, 'd', simulate_input::ALT),
      delay);

  const unsigned int kInterval = 500;
  int next_delay = delay + kInterval;

  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
      simulate_input::SendStringW, url), next_delay);

  next_delay = next_delay + kInterval;

  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
      simulate_input::SendCharA, VK_RETURN, simulate_input::NONE),
      next_delay);
}

ACTION_P5(ValidateWindowSize, mock, left, top, width, height) {
  int actual_left = 0;
  int actual_top = 0;
  int actual_width = 0;
  int actual_height = 0;

  IWebBrowser2* web_browser2 = mock->event_sink()->web_browser2();
  web_browser2->get_Left(reinterpret_cast<long*>(&actual_left));  // NOLINT
  web_browser2->get_Top(reinterpret_cast<long*>(&actual_top));  // NOLINT
  web_browser2->get_Width(reinterpret_cast<long*>(&actual_width));  // NOLINT
  web_browser2->get_Height(reinterpret_cast<long*>(&actual_height));  // NOLINT

  EXPECT_EQ(actual_left, left);
  EXPECT_EQ(actual_top, top);

  EXPECT_LE(actual_width, width + 20);
  EXPECT_GT(actual_width, width - 20);
  EXPECT_LE(actual_height, height + 100);
  EXPECT_GT(actual_height, height - 100);
}

ACTION_P(VerifyAddressBarUrlWithGcf, mock) {
  std::wstring expected_url = L"gcf:";
  expected_url += arg1;
  mock->event_sink()->ExpectAddressBarUrl(expected_url);
}

// Polls to see if the file is saved and closes the browser once it is.
// This doesn't do any checking of the contents of the file.
ACTION_P3(CloseWhenFileSaved, mock, file, timeout_ms) {
  base::Time start = base::Time::Now();
  while (!file_util::PathExists(file)) {
    PlatformThread::Sleep(200);
    if ((base::Time::Now() - start).InMilliseconds() > timeout_ms) {
      ADD_FAILURE() << "File was not saved within timeout";
      break;
    }
  }
  mock->event_sink()->CloseWebBrowser();
}

ACTION_P2(OpenContextMenu, loop, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
      simulate_input::SendScanCode, VK_F10, simulate_input::SHIFT), delay);
}

ACTION_P3(SelectItem, loop, delay, index) {
  chrome_frame_test::DelaySendExtendedKeysEnter(loop, delay, VK_DOWN, index + 1,
      simulate_input::NONE);
}

ACTION(FocusWindow) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, AccObjectMatcher(), &object))
    object->Focus();
}

ACTION_P(DoDefaultAction, matcher) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object))
    object->DoDefaultAction();
}

ACTION_P(FocusAccObject, matcher) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object))
    object->Focus();
}

ACTION_P(SelectAccObject, matcher) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object))
    object->Select();
}

ACTION_P2(SetAccObjectValue, matcher, value) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object))
    object->SetValue(value);
}

ACTION(OpenContextMenuAsync) {
  // Special case this implementation because the top-left of the window is
  // much more likely to be empty than the center.
  HWND hwnd = arg0;
  scoped_refptr<AccObject> object;
  // TODO(kkania): Switch to using WM_CONTEXTMENU with coordinates -1, -1
  // when Chrome supports this. See render_widget_host_view_win.h.
  LPARAM coordinates = (1 << 16) | 1;
  // IE needs both messages in order to work.
  ::PostMessage(hwnd, WM_RBUTTONDOWN, (WPARAM)0, coordinates);
  ::PostMessage(hwnd, WM_RBUTTONUP, (WPARAM)0, coordinates);
}

ACTION_P(OpenContextMenuAsync, matcher) {
  HWND hwnd = arg0;
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(hwnd, matcher, &object)) {
    // TODO(kkania): Switch to using WM_CONTEXTMENU with coordinates -1, -1
    // when Chrome supports this. See render_widget_host_view_win.h.
    gfx::Rect object_rect;
    bool got_location = object->GetLocation(&object_rect);
    EXPECT_TRUE(got_location) << "Could not get bounding rect for object: "
        << object->GetDescription();
    if (got_location) {
      // WM_RBUTTON* messages expect a relative coordinate, while GetLocation
      // returned a screen coordinate. Adjust.
      RECT rect;
      BOOL got_window_rect = ::GetWindowRect(hwnd, &rect);
      EXPECT_TRUE(got_window_rect) << "Could not get window bounds";
      if (got_window_rect) {
        gfx::Rect window_rect(rect);
        gfx::Point relative_origin =
            object_rect.CenterPoint().Subtract(window_rect.origin());
        LPARAM coordinates = (relative_origin.y() << 16) | relative_origin.x();
        // IE needs both messages in order to work.
        ::PostMessage(hwnd, WM_RBUTTONDOWN, (WPARAM)0, coordinates);
        ::PostMessage(hwnd, WM_RBUTTONUP, (WPARAM)0, coordinates);
      }
    }
  }
}

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_ACTIONS_H_

