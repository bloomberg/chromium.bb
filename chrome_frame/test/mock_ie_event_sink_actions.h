// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_ACTIONS_H_
#define CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_ACTIONS_H_

#include <windows.h>
#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "base/test/test_process_killer_win.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/test/chrome_frame_ui_test_utils.h"
#include "chrome_frame/test/ie_event_sink.h"
#include "chrome_frame/test/mock_ie_event_sink_test.h"
#include "chrome_frame/test/simulate_input.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace chrome_frame_test {

MATCHER_P(UrlPathEq, url, "equals the path and query portion of the url") {
  return arg == chrome_frame_test::GetPathAndQueryFromUrl(url);
}

MATCHER_P(AccSatisfies, matcher, "satisfies the given AccObjectMatcher") {
  return matcher.DoesMatch(arg);
}

// Returns true if the title of the page rendered in the window |arg| equals
// |the_title|. For pages rendered in Chrome, the title of the parent of |arg|
// is the page title. For pages rendered in IE, the title of the grandparent of
// |arg| begins with the page title. To handle both cases, attempt a prefix
// match on each window starting with the parent of |arg|.
MATCHER_P(TabContentsTitleEq, the_title, "") {
  const string16 title(the_title);
  DCHECK(!title.empty());
  HWND parent = GetParent(arg);
  if (parent != NULL) {
    string16 parent_title(255, L'\0');
    std::ostringstream titles_found(std::string("titles found: "));
    string16 first_title;
    do {
      parent_title.resize(255, L'\0');
      parent_title.resize(GetWindowText(parent, &parent_title[0],
                                        parent_title.size()));
      if (parent_title.size() >= title.size() &&
          std::equal(title.begin(), title.end(), parent_title.begin())) {
          return true;
      }
      titles_found << "\"" << UTF16ToASCII(parent_title) << "\" ";
      parent = GetParent(parent);
    } while(parent != NULL);
    *result_listener << titles_found.str();
  } else {
    *result_listener << "the window has no parent";
  }
  return false;
}

// IWebBrowser2 actions

ACTION_P2(Navigate, mock, navigate_url) {
  mock->event_sink()->Navigate(navigate_url);
}

ACTION_P3(DelayNavigateToCurrentUrl, mock, loop, delay) {
  loop->PostDelayedTask(FROM_HERE,
                        base::Bind(&NavigateToCurrentUrl, mock), delay);
}

ACTION_P(CloseBrowserMock, mock) {
  mock->event_sink()->CloseWebBrowser();
}

ACTION_P3(DelayCloseBrowserMock, loop, delay, mock) {
  loop->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&IEEventSink::CloseWebBrowser),
                 mock->event_sink()),
      delay);
}

ACTION_P2(ConnectDocPropNotifySink, mock, sink) {
  base::win::ScopedComPtr<IDispatch> document;
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
  loop->PostDelayedTask(
      FROM_HERE,
      base::Bind(&IEEventSink::Exec, mock->event_sink(), cmd_group_guid, cmd_id,
                 cmd_exec_opt, in_args, out_args),
      delay);
}

ACTION_P3(DelayGoBack, mock, loop, delay) {
  loop->PostDelayedTask(
      FROM_HERE, base::Bind(&IEEventSink::GoBack, mock->event_sink()), delay);
}

ACTION_P3(DelayGoForward, mock, loop, delay) {
  loop->PostDelayedTask(
      FROM_HERE, base::Bind(&IEEventSink::GoForward, mock->event_sink()),
      delay);
}

ACTION_P3(DelayRefresh, mock, loop, delay) {
  loop->PostDelayedTask(
      FROM_HERE, base::Bind(&IEEventSink::Refresh, mock->event_sink()), delay);
}

ACTION_P2(PostMessageToCF, mock, message) {
  mock->event_sink()->PostMessageToCF(message, L"*");
}

// Accessibility-related actions

ACTION_P(AccDoDefaultAction, matcher) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object)) {
    EXPECT_TRUE(object->DoDefaultAction());
  }
}

ACTION_P2(DelayAccDoDefaultAction, matcher, delay) {
  SleepEx(delay, false);
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

ACTION_P(AccSendCommand, matcher) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object)) {
    HWND window = NULL;
    object->GetWindow(&window);
    long window_id = GetWindowLong(window, GWL_ID);
    ::SendMessage(arg0, WM_COMMAND, MAKEWPARAM(window_id, BN_CLICKED),
                  reinterpret_cast<LPARAM>(window));
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

namespace { // NOLINT
template<typename R> R AccInWindow(testing::Action<R(HWND)> action, HWND hwnd) {
  return action.Perform(typename testing::Action<R(HWND)>::ArgumentTuple(hwnd));
}
}

ACTION_P2(AccDoDefaultActionInBrowser, mock, matcher) {
  AccInWindow<void>(AccDoDefaultAction(matcher),
                    mock->event_sink()->GetBrowserWindow());
}

ACTION_P2(AccDoDefaultActionInRenderer, mock, matcher) {
  AccInWindow<void>(AccDoDefaultAction(matcher),
                    mock->event_sink()->GetRendererWindow());
}

ACTION_P3(DelayAccDoDefaultActionInRenderer, mock, matcher, delay) {
  SleepEx(delay, false);
  AccInWindow<void>(AccDoDefaultAction(matcher),
                    mock->event_sink()->GetRendererWindow());
}

ACTION_P2(AccLeftClickInBrowser, mock, matcher) {
  AccInWindow<void>(AccLeftClick(matcher),
                    mock->event_sink()->GetBrowserWindow());
}

ACTION_P2(AccLeftClickInRenderer, mock, matcher) {
  AccInWindow<void>(AccLeftClick(matcher),
                    mock->event_sink()->GetRendererWindow());
}

ACTION_P3(AccSetValueInBrowser, mock, matcher, value) {
  AccInWindow<void>(AccSetValue(matcher, value),
                    mock->event_sink()->GetBrowserWindow());
}

ACTION_P2(AccWatchForOneValueChange, observer, matcher) {
  observer->WatchForOneValueChange(matcher);
}

ACTION_P2(AccSendCharMessage, matcher, character_code) {
  scoped_refptr<AccObject> object;
  if (FindAccObjectInWindow(arg0, matcher, &object)) {
    HWND window = NULL;
    EXPECT_TRUE(object->GetWindow(&window));
    ::SendMessage(window, WM_CHAR, character_code, 0);
  }
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
  AccInWindow<void>(PostCharMessage(character_code),
                    mock->event_sink()->GetRendererWindow());
}

ACTION_P2(PostCharMessagesToRenderer, mock, character_codes) {
  HWND window = mock->event_sink()->GetRendererWindow();
  std::string codes = character_codes;
  for (size_t i = 0; i < codes.length(); i++)
    ::PostMessage(window, WM_CHAR, codes[i], 0);
}

ACTION_P3(WatchWindow, mock, caption, window_class) {
  mock->WatchWindow(caption, window_class);
}

ACTION_P(StopWindowWatching, mock) {
  mock->StopWatching();
}

ACTION_P(WatchWindowProcess, mock_observer) {
  mock_observer->WatchProcessForHwnd(arg0);
}

ACTION_P2(WatchBrowserProcess, mock_observer, mock) {
  AccInWindow<void>(WatchWindowProcess(mock_observer),
                    mock->event_sink()->GetBrowserWindow());
}

ACTION_P2(WatchRendererProcess, mock_observer, mock) {
  AccInWindow<void>(WatchWindowProcess(mock_observer),
                    mock->event_sink()->GetRendererWindow());
}

namespace { // NOLINT

void DoCloseWindowNow(HWND hwnd) {
  ::PostMessage(hwnd, WM_CLOSE, 0, 0);
}

}  // namespace

ACTION(DoCloseWindow) {
  DoCloseWindowNow(arg0);
}

ACTION_P(DelayDoCloseWindow, delay) {
  DCHECK(MessageLoop::current());
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::Bind(DoCloseWindowNow, arg0),
      base::TimeDelta::FromMilliseconds(delay));
}

ACTION(KillChromeFrameProcesses) {
  base::KillAllNamedProcessesWithArgument(
      UTF8ToWide(chrome_frame_test::kChromeImageName),
      UTF8ToWide(switches::kChromeFrame));
}

// Verifying actions
ACTION_P(AccExpect, matcher) {
  scoped_refptr<AccObject> object;
  EXPECT_TRUE(FindAccObjectInWindow(arg0, matcher, &object));
}

ACTION_P2(AccExpectInRenderer, mock, matcher) {
  AccInWindow<void>(AccExpect(matcher),
                    mock->event_sink()->GetRendererWindow());
}

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

  EXPECT_GE(actual_left, left);
  EXPECT_GE(actual_top, top);

  EXPECT_GE(actual_width, width);
  EXPECT_GE(actual_height, height);
}

ACTION_P(VerifyAddressBarUrlWithGcf, mock) {
  std::wstring expected_url = L"gcf:";
  expected_url += arg1;
  mock->event_sink()->ExpectAddressBarUrl(expected_url);
}

ACTION_P2(ExpectDocumentReadystate, mock, ready_state) {
  mock->ExpectDocumentReadystate(ready_state);
}

ACTION_P(VerifySelectedText, expected_text) {
  std::wstring actual_text;
  bool got_selection = arg1->GetSelectedText(&actual_text);
  EXPECT_TRUE(got_selection);
  if (got_selection) {
    EXPECT_EQ(expected_text, actual_text);
  }
}

// Polling actions

ACTION_P3(CloseWhenFileSaved, mock, file, timeout_ms) {
  base::Time start = base::Time::Now();
  while (!file_util::PathExists(file)) {
    if ((base::Time::Now() - start).InMilliseconds() > timeout_ms) {
      ADD_FAILURE() << "File was not saved within timeout";
      break;
    }
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
  }
  mock->event_sink()->CloseWebBrowser();
}

ACTION_P2(WaitForFileSave, file, timeout_ms) {
  base::Time start = base::Time::Now();
  while (!file_util::PathExists(file)) {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
    if ((base::Time::Now() - start).InMilliseconds() > timeout_ms) {
      ADD_FAILURE() << "File was not saved within timeout";
      break;
    }
  }
}

// Flaky actions

ACTION_P(SetFocusToRenderer, mock) {
  simulate_input::SetKeyboardFocusToWindow(
      mock->event_sink()->GetRendererWindow());
}

ACTION_P4(DelaySendChar, loop, delay, c, mod) {
  loop->PostDelayedTask(
      FROM_HERE, base::Bind(simulate_input::SendCharA, c, mod), delay);
}

ACTION_P4(DelaySendScanCode, loop, delay, c, mod) {
  loop->PostDelayedTask(
      FROM_HERE, base::Bind(simulate_input::SendScanCode, c, mod), delay);
}

// This function selects the address bar via the Alt+d shortcut.
ACTION_P3(TypeUrlInAddressBar, loop, url, delay) {
  loop->PostDelayedTask(
      FROM_HERE,
      base::Bind(simulate_input::SendCharA, 'd', simulate_input::ALT), delay);

  const base::TimeDelta kInterval = base::TimeDelta::FromMilliseconds(500);
  base::TimeDelta next_delay = delay + kInterval;

  loop->PostDelayedTask(
      FROM_HERE, base::Bind(simulate_input::SendStringW, url), next_delay);

  next_delay = next_delay + kInterval;

  loop->PostDelayedTask(
      FROM_HERE,
      base::Bind(simulate_input::SendCharA, VK_RETURN, simulate_input::NONE),
      next_delay);
}

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_MOCK_IE_EVENT_SINK_ACTIONS_H_
