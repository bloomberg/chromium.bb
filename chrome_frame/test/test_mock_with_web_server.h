// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_MOCK_WITH_WEB_SERVER_H_
#define CHROME_FRAME_TEST_MOCK_WITH_WEB_SERVER_H_

#include <windows.h>

#include "gmock/gmock.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"

#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

namespace chrome_frame_test {

using ::testing::Expectation;
using ::testing::ExpectationSet;

// This class provides functionality to add expectations to IE full tab mode
// tests.
class MockWebBrowserEventSink : public chrome_frame_test::WebBrowserEventSink {
 public:
  // Needed to support PostTask.
  static bool ImplementsThreadSafeReferenceCounting() {
    return true;
  }

  MOCK_METHOD7_WITH_CALLTYPE(__stdcall, OnBeforeNavigate2,
                             void (IDispatch* dispatch,  // NOLINT
                                      VARIANT* url,
                                      VARIANT* flags,
                                      VARIANT* target_frame_name,
                                      VARIANT* post_data,
                                      VARIANT* headers,
                                      VARIANT_BOOL* cancel));

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, OnNavigateComplete2,
                             void (IDispatch* dispatch,  // NOLINT
                                   VARIANT* url));

  MOCK_METHOD5_WITH_CALLTYPE(__stdcall, OnNewWindow3,
                             void (IDispatch** dispatch,  // NOLINT
                                   VARIANT_BOOL* cancel,
                                   DWORD flags,
                                   BSTR url_context,
                                   BSTR url));

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, OnNewWindow2,
                             void (IDispatch** dispatch,  // NOLINT
                                   VARIANT_BOOL* cancel));

  MOCK_METHOD5_WITH_CALLTYPE(__stdcall, OnNavigateError,
                             void (IDispatch* dispatch,  // NOLINT
                                   VARIANT* url,
                                   VARIANT* frame_name,
                                   VARIANT* status_code,
                                   VARIANT* cancel));

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, OnFileDownload,
                             void (VARIANT_BOOL active_doc,
                                   VARIANT_BOOL* cancel));

  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, OnQuit,
                             void ());

  MOCK_METHOD1(OnLoad, void (const wchar_t* url));  // NOLINT
  MOCK_METHOD1(OnLoadError, void (const wchar_t* url));  // NOLINT
  MOCK_METHOD3(OnMessage, void (const wchar_t* message,
                                const wchar_t* origin,
                                const wchar_t* source));  // NOLINT
  MOCK_METHOD2(OnNewBrowserWindow, void (IDispatch* dispatch,  // NOLINT
                                         const wchar_t* url));
  MOCK_METHOD2(OnWindowDetected, void (HWND hwnd,  // NOLINT
                                       const std::string& caption));

  // Test expectations for general navigations.
  ExpectationSet ExpectNavigationCardinality(const std::wstring& url,
                                             testing::Cardinality cardinality);
  ExpectationSet ExpectNavigation(const std::wstring& url);
  ExpectationSet ExpectNavigationAndSwitch(const std::wstring& url);
  ExpectationSet ExpectNavigationAndSwitchSequence(const std::wstring& url);
  ExpectationSet ExpectNewWindow(MockWebBrowserEventSink* new_window_mock);
  ExpectationSet ExpectNavigationSequenceForAnchors(const std::wstring& url);

  // Test expectations for navigations with an IE renderer.
  // Expect one navigation to occur.
  ExpectationSet ExpectNavigationInIE(const std::wstring& url);
  // Expect a new window to be opened to |url|. Set |new_window_mock| as the new
  // window.
  ExpectationSet ExpectNewWindowWithIE(
      const std::wstring& url, MockWebBrowserEventSink* new_window_mock);
};

ACTION_P(CloseBrowserMock, mock) {
  mock->CloseWebBrowser();
}

ACTION_P(VerifyAddressBarUrl, mock) {
  mock->ExpectAddressBarUrl(std::wstring(arg0));
}

ACTION_P4(DelaySendScanCode, loop, delay, c, mod) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
        simulate_input::SendScanCode, c, mod), delay);
}

ACTION_P5(ValidateWindowSize, mock, left, top, width, height) {
  long actual_left = 0;
  long actual_top = 0;
  long actual_width = 0;
  long actual_height = 0;

  mock->web_browser2()->get_Left(&actual_left);
  mock->web_browser2()->get_Top(&actual_top);
  mock->web_browser2()->get_Width(&actual_width);
  mock->web_browser2()->get_Height(&actual_height);

  EXPECT_EQ(actual_left, left);
  EXPECT_EQ(actual_top, top);
  EXPECT_EQ(actual_width, width);
  EXPECT_EQ(actual_height, height);
}

ACTION_P4(DelaySendChar, loop, delay, c, mod) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
      simulate_input::SendCharA, c, mod), delay);
}

ACTION_P3(DelayCloseBrowserMock, loop, delay, mock) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock,
      &MockWebBrowserEventSink::CloseWebBrowser), delay);
}

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_MOCK_WITH_WEB_SERVER_H_

