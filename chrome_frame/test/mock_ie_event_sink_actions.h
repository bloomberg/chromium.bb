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

// IWebBrowser2 actions

ACTION_P2(Navigate, mock, navigate_url) {
  mock->event_sink()->Navigate(navigate_url);
}

ACTION_P3(DelayNavigateToCurrentUrl, mock, loop, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(&NavigateToCurrentUrl,
      mock), delay);
}

ACTION_P(CloseBrowserMock, mock) {
  mock->event_sink()->CloseWebBrowser();
}

ACTION_P3(DelayCloseBrowserMock, loop, delay, mock) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::CloseWebBrowser), delay);
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

ACTION_P8(DelayExecCommand, mock, loop, delay, cmd_group_guid, cmd_id,
          cmd_exec_opt, in_args, out_args) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::Exec, cmd_group_guid, cmd_id, cmd_exec_opt, in_args,
          out_args), delay);
}

ACTION_P3(DelayGoBack, mock, loop, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::GoBack), delay);
}

ACTION_P3(DelayGoForward, mock, loop, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::GoForward), delay);
}

ACTION_P3(DelayRefresh, mock, loop, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock->event_sink(),
      &IEEventSink::Refresh), delay);
}

// Accessibility-related actions

ACTION_P(AccDoDefaultAction, matcher) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object)) {
    EXPECT_TRUE(object->DoDefaultAction());
  }
}

ACTION_P(AccLeftClick, matcher) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object)) {
    EXPECT_TRUE(object->LeftClick());
  }
}

ACTION_P(AccRightClick, matcher) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object)) {
    EXPECT_TRUE(object->RightClick());
  }
}

ACTION_P(AccFocus, matcher) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object)) {
    EXPECT_TRUE(object->Focus());
  }
}

ACTION_P(AccSelect, matcher) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object)) {
    EXPECT_TRUE(object->Select());
  }
}

ACTION_P2(AccSetValue, matcher, value) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object)) {
    EXPECT_TRUE(object->SetValue(value));
  }
}

ACTION_P2(AccDoDefaultActionInRenderer, mock, matcher) {
  HWND window = mock->event_sink()->GetRendererWindow();
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(window, matcher, &object)) {
    EXPECT_TRUE(object->DoDefaultAction());
  }
}

ACTION_P2(AccLeftClickInBrowser, mock, matcher) {
  HWND window = mock->event_sink()->GetBrowserWindow();
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(window, matcher, &object)) {
    EXPECT_TRUE(object->LeftClick());
  }
}

ACTION_P3(AccSetValueInBrowser, mock, matcher, value) {
  HWND window = mock->event_sink()->GetBrowserWindow();
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(window, matcher, &object)) {
    EXPECT_TRUE(object->SetValue(value));
  }
}

ACTION_P2(AccWatchForOneValueChange, observer, matcher) {
  observer->WatchForOneValueChange(matcher);
}

// Various other actions

ACTION(OpenContextMenuAsync) {
  // Special case this implementation because the top-left of the window is
  // much more likely to be empty than the center.
  HWND hwnd = arg0;
  LPARAM coordinates = (1 << 16) | 1;
  // IE needs both messages in order to work. Chrome does not support
  // WM_CONTEXTMENU in the renderer: http://crbug.com/51746.
  ::PostMessage(hwnd, WM_RBUTTONDOWN, 0, coordinates);
  ::PostMessage(hwnd, WM_RBUTTONUP, 0, coordinates);
}

ACTION_P(PostCharMessage, character_code) {
  ::PostMessage(arg0, WM_CHAR, character_code, 0);
}

ACTION_P2(PostCharMessageToRenderer, mock, character_code) {
  HWND window = mock->event_sink()->GetRendererWindow();
  ::PostMessage(window, WM_CHAR, character_code, 0);
}

ACTION_P2(PostCharMessagesToRenderer, mock, character_codes) {
  HWND window = mock->event_sink()->GetRendererWindow();
  std::string codes = character_codes;
  for (size_t i = 0; i < codes.length(); i++)
    ::PostMessage(window, WM_CHAR, codes[i], 0);
}

ACTION_P2(WatchWindow, mock, window_class) {
  mock->WatchWindow(window_class);
}

ACTION_P(StopWindowWatching, mock) {
  mock->StopWatching();
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

ACTION(KillChromeFrameProcesses) {
  KillAllNamedProcessesWithArgument(
      UTF8ToWide(chrome_frame_test::kChromeImageName),
      UTF8ToWide(switches::kChromeFrame));
}

// Verifying actions

ACTION_P(ExpectRendererHasFocus, mock) {
  mock->event_sink()->ExpectRendererWindowHasFocus();
}

ACTION_P(VerifyAddressBarUrl, mock) {
  mock->event_sink()->ExpectAddressBarUrl(std::wstring(arg1));
}

ACTION_P3(VerifyPageLoad, mock, in_cf, url) {
  EXPECT_TRUE(static_cast<bool>(in_cf) == mock->event_sink()->IsCFRendering());
  mock->event_sink()->ExpectAddressBarUrl(url);
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

ACTION_P2(ExpectDocumentReadystate, mock, ready_state) {
  mock->ExpectDocumentReadystate(ready_state);
}

// Polling actions

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

// Flaky actions

ACTION_P(SetFocusToRenderer, mock) {
  simulate_input::SetKeyboardFocusToWindow(
      mock->event_sink()->GetRendererWindow());
}

ACTION_P4(DelaySendChar, loop, delay, c, mod) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
      simulate_input::SendCharA, c, mod), delay);
}

ACTION_P4(DelaySendScanCode, loop, delay, c, mod) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
        simulate_input::SendScanCode, c, mod), delay);
}

// This function selects the address bar via the Alt+d shortcut.
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

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_ACTIONS_H_
