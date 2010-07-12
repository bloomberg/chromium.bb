// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_ACTIONS_H_
#define CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_ACTIONS_H_

#include "chrome_frame/test/simulate_input.h"
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

ACTION(DoCloseWindow) {
  ::PostMessage(arg0, WM_SYSCOMMAND, SC_CLOSE, 0);
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

ACTION_P4(DelaySendScanCode, loop, delay, c, mod) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
        simulate_input::SendScanCode, c, mod), delay);
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
  web_browser2->get_Left(reinterpret_cast<long*>(&actual_left));
  web_browser2->get_Top(reinterpret_cast<long*>(&actual_top));
  web_browser2->get_Width(reinterpret_cast<long*>(&actual_width));
  web_browser2->get_Height(reinterpret_cast<long*>(&actual_height));

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

ACTION_P2(OpenContextMenu, loop, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
      simulate_input::SendScanCode, VK_F10, simulate_input::SHIFT), delay);
}

ACTION_P3(SelectItem, loop, delay, index) {
  chrome_frame_test::DelaySendExtendedKeysEnter(loop, delay, VK_DOWN, index + 1,
      simulate_input::NONE);
}

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_ACTIONS_H_