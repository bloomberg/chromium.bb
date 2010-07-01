// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome_frame/test/test_mock_with_web_server.h"

#include <mshtmcid.h>

#include "base/scoped_bstr_win.h"
#include "chrome/common/url_constants.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/test/simulate_input.h"
#include "chrome_frame/test/test_with_web_server.h"

#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::_;
using testing::CreateFunctor;
using chrome_frame_test::CloseIeAtEndOfScope;
using chrome_frame_test::ComStackObjectWithUninitialize;
using chrome_frame_test::kChromeFrameLongNavigationTimeoutInSeconds;
using chrome_frame_test::MockWebBrowserEventSink;

const wchar_t kChromeFrameFileUrl[] = L"gcf:file:///C:/";
const wchar_t enter_key[] = { VK_RETURN, 0 };
const wchar_t escape_key[] = { VK_ESCAPE, 0 };
const wchar_t tab_enter_keys[] = { VK_TAB, VK_RETURN, 0 };

namespace chrome_frame_test {

ExpectationSet MockWebBrowserEventSink::ExpectNavigationCardinality(
    const std::wstring& url, testing::Cardinality cardinality) {
  // Expect a single navigation sequence. If URL is specified,
  // match the URL.
  // The navigation sequence is a set of OnBeforeNavigate2, OnFileDownload
  // and OnNavigationComplete2 events.  For certain navigations, internal
  // vs external (and maybe between different IE versions) these events
  // events occur with different frequencies. Hence, the variable
  // cardinality.
  ExpectationSet navigation;
  if (url.empty()) {
    navigation += EXPECT_CALL(*this, OnBeforeNavigate2(_, _, _, _, _, _, _))
        .Times(cardinality);
  } else {
    navigation += EXPECT_CALL(*this, OnBeforeNavigate2(_,
                              testing::Field(&VARIANT::bstrVal,
                              testing::StrCaseEq(url)), _, _, _, _, _))
        .Times(cardinality);
  }
  navigation += EXPECT_CALL(*this, OnFileDownload(VARIANT_TRUE, _))
      .Times(cardinality);

  if (url.empty()) {
    navigation += EXPECT_CALL(*this, OnNavigateComplete2(_, _))
        .Times(cardinality);
  } else {
    navigation += EXPECT_CALL(*this, OnNavigateComplete2(_,
                              testing::Field(&VARIANT::bstrVal,
                              testing::StrCaseEq(url))))
        .Times(cardinality);
  }

  return navigation;
}

ExpectationSet MockWebBrowserEventSink::ExpectNavigation(
    const std::wstring& url) {
  // When the onhttpequiv patch is enabled, we will get two
  // BeforeNavigate2/OnNavigateComplete2 notifications due to
  // switching from IE to CF.
  // Note that when going backwards, we don't expect that since the extra
  // navigational entries in the travel log should have been removed.
  ExpectationSet navigation;
  navigation += EXPECT_CALL(*this, OnBeforeNavigate2(_,
                            testing::Field(&VARIANT::bstrVal,
                            testing::StrCaseEq(url)), _, _, _, _, _));
  navigation += EXPECT_CALL(*this, OnFileDownload(VARIANT_TRUE, _))
      .Times(testing::AnyNumber());
  navigation += EXPECT_CALL(*this, OnNavigateComplete2(_,
                            testing::Field(&VARIANT::bstrVal,
                            testing::StrCaseEq(url))))
      .Times(testing::AnyNumber());

  return navigation;
}

ExpectationSet MockWebBrowserEventSink::ExpectNavigationSequenceForAnchors(
    const std::wstring& url) {
  // When the onhttpequiv patch is enabled, we will get two
  // BeforeNavigate2/OnNavigateComplete2 notifications due to
  // switching from IE to CF.
  // Note that when going backwards, we don't expect that since the extra
  // navigational entries in the travel log should have been removed.
  // On IE6 we don't receive the BeforeNavigate notifications for anchors.
  ExpectationSet navigation;
  navigation += EXPECT_CALL(*this, OnBeforeNavigate2(_,
                            testing::Field(&VARIANT::bstrVal,
                            testing::StrCaseEq(url)), _, _, _, _, _))
      .Times(testing::AnyNumber());
  navigation += EXPECT_CALL(*this, OnFileDownload(VARIANT_TRUE, _))
      .Times(testing::AnyNumber());
  navigation += EXPECT_CALL(*this, OnNavigateComplete2(_,
                            testing::Field(&VARIANT::bstrVal,
                            testing::StrCaseEq(url))))
      .Times(testing::AnyNumber());

  return navigation;
}

ExpectationSet MockWebBrowserEventSink::ExpectNavigationAndSwitch(
    const std::wstring& url) {
  return ExpectNavigationCardinality(url, testing::AnyNumber());
}

ExpectationSet MockWebBrowserEventSink::ExpectNavigationAndSwitchSequence(
    const std::wstring& url) {
  // When navigation expectations occur in sequence the following order
  // is necessary. This is mainly based on observed quirks rather than
  // any theory.
  // TODO(joshia): Improve expectations here
  ExpectationSet navigation = ExpectNavigationCardinality(url,
                                                          testing::Exactly(1));
  navigation += EXPECT_CALL(*this, OnBeforeNavigate2(_,
                            testing::Field(&VARIANT::bstrVal,
                            testing::StrCaseEq(url)), _, _, _, _, _))
      .Times(testing::AnyNumber());
  navigation += EXPECT_CALL(*this, OnFileDownload(VARIANT_TRUE, _))
      .Times(testing::AnyNumber());
  navigation += EXPECT_CALL(*this, OnNavigateComplete2(_,
                            testing::Field(&VARIANT::bstrVal,
                            testing::StrCaseEq(url))))
      .Times(testing::AnyNumber());

  return navigation;
}

ExpectationSet MockWebBrowserEventSink::ExpectNewWindow(
    MockWebBrowserEventSink* new_window_mock) {
  DCHECK(new_window_mock);
  ExpectationSet new_window;
  new_window += EXPECT_CALL(*this, OnNewWindow3(_, _, _, _, _));
  new_window += EXPECT_CALL(*this, OnNewBrowserWindow(_, _))
      .WillOnce(testing::WithArgs<0>(testing::Invoke(CreateFunctor(
                new_window_mock, &MockWebBrowserEventSink::Attach))));

  new_window_mock->ExpectNavigationAndSwitch(std::wstring());
  return new_window;
}

ExpectationSet MockWebBrowserEventSink::ExpectNavigationInIE(
    const std::wstring& url) {
  testing::InSequence sequence;
  ExpectationSet navigation;
  navigation += EXPECT_CALL(*this, OnBeforeNavigate2(_,
                            testing::Field(&VARIANT::bstrVal,
                            testing::StrCaseEq(url)), _, _, _, _, _));
  navigation += EXPECT_CALL(*this, OnFileDownload(VARIANT_TRUE, _))
      .Times(testing::AnyNumber());
  navigation += EXPECT_CALL(*this, OnNavigateComplete2(_,
                            testing::Field(&VARIANT::bstrVal,
                            testing::StrCaseEq(url))));
  return navigation;
}

ExpectationSet MockWebBrowserEventSink::ExpectNewWindowWithIE(
    const std::wstring& url, MockWebBrowserEventSink* new_window_mock) {
  DCHECK(new_window_mock);
  ExpectationSet new_window;
  new_window += EXPECT_CALL(*this, OnNewWindow3(_, _, _, _, _));
  new_window += EXPECT_CALL(*this, OnNewBrowserWindow(_, _))
      .WillOnce(testing::WithArgs<0>(testing::Invoke(CreateFunctor(
                new_window_mock, &MockWebBrowserEventSink::Attach))));

  new_window_mock->ExpectNavigationInIE(url);
  return new_window;
}

}  // namespace chrome_frame_test

ACTION_P(SetFocusToChrome, mock) {
  mock->SetFocusToChrome();
}

ACTION_P2(Navigate, mock, url) {
  mock->Navigate(url);
}

ACTION_P2(WatchWindow, mock, window_class) {
  mock->WatchChromeWindow(window_class);
}

ACTION_P(StopWindowWatching, mock) {
  mock->StopWatching();
}

ACTION_P6(DelaySendMouseClick, mock, loop, delay, x, y, button) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock,
      &MockWebBrowserEventSink::SendMouseClick, x, y, button), delay);
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

ACTION_P(VerifyAddressBarUrlWithGcf, mock) {
  std::wstring expected_url = L"gcf:";
  expected_url += arg0;
  mock->ExpectAddressBarUrl(expected_url);
}

TEST(ChromeFrameTest, FullTabModeIE_DisallowedUrls) {
  CloseIeAtEndOfScope last_resort_close_ie;
  // If a navigation fails then IE issues a navigation to an interstitial
  // page. Catch this to track navigation errors as the NavigateError
  // notification does not seem to fire reliably.
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;

  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kChromeFrameFileUrl)),
                                      _, _, _, _, _));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StartsWith(L"res:")),
                                      _, _, _, _, _));
  EXPECT_CALL(mock, OnFileDownload(VARIANT_TRUE, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return());
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(CloseBrowserMock(&mock));
  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1)).WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kChromeFrameFileUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

const wchar_t kKeyEventUrl[] = L"http://localhost:1337/files/keyevent.html";

// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_KeyboardTest) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;

  mock.ExpectNavigationAndSwitch(kKeyEventUrl);

  const wchar_t* input = L"Chrome";
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kKeyEventUrl)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::LEFT),
          DelaySendString(&loop, 500, input)));

  EXPECT_CALL(mock, OnMessage(testing::StrEq(input), _, _))
      .WillOnce(CloseBrowserMock(&mock));
  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kKeyEventUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

const wchar_t kAboutVersionUrl[] = L"gcf:about:version";
const wchar_t kAboutVersion[] = L"about:version";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_FocusTest) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;

  mock.ExpectNavigationAndSwitch(kAboutVersionUrl);

  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAboutVersion)))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&mock,
              &MockWebBrowserEventSink::ExpectRendererWindowHasFocus)),
          VerifyAddressBarUrlWithGcf(&mock),
          CloseBrowserMock(&mock)));

  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kAboutVersionUrl);

  // Allow some time for chrome to be launched.
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

const wchar_t kWindowOpenUrl[] =
    L"http://localhost:1337/files/chrome_frame_window_open.html";

const wchar_t kWindowOpenPopupUrl[] =
    L"http://localhost:1337/files/chrome_frame_window_open_popup.html";

// This test checks if window.open calls issued by a full tab mode ChromeFrame
// instance make it back to IE and then transitions back to Chrome as the
// window.open target page is supposed to render within Chrome.
// Marking this test as FLAKY initially as it relies on getting focus and user
// input which don't work correctly at times.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_WindowOpenInChrome) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> new_window_mock;
  chrome_frame_test::TimedMsgLoop loop;

  mock.ExpectNavigationAndSwitch(kWindowOpenUrl);

  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kWindowOpenUrl)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::LEFT),
          DelaySendChar(&loop, 500, 'O', simulate_input::NONE)));
  // Watch for new window
  mock.ExpectNewWindow(&new_window_mock);

  EXPECT_CALL(new_window_mock, OnLoad(testing::StrCaseEq(kWindowOpenPopupUrl)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&new_window_mock),
          ValidateWindowSize(&new_window_mock, 10, 10, 250, 250),
          CloseBrowserMock(&new_window_mock)));

  EXPECT_CALL(new_window_mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(CloseBrowserMock(&mock));

  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kWindowOpenUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  ASSERT_TRUE(new_window_mock.web_browser2() != NULL);
}

const wchar_t kSubFrameUrl1[] =
    L"http://localhost:1337/files/sub_frame1.html";
const wchar_t kSubFrameUrl2[] =
    L"http://localhost:1337/files/sub_frame2.html";
const wchar_t kSubFrameUrl3[] =
    L"http://localhost:1337/files/sub_frame3.html";

// Test new window behavior with ctrl+N
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_CtrlN) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;

  // Ideally we want to use a mock to watch for finer grained
  // events for New Window, but for Crl+N we don't get any
  // OnNewWindowX notifications. :(
  const wchar_t* kIEFrameClass = L"IEFrame";
  mock.ExpectNavigationAndSwitch(kKeyEventUrl);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kKeyEventUrl)))
      .WillOnce(testing::DoAll(
          WatchWindow(&mock, kIEFrameClass),
          SetFocusToChrome(&mock),
          DelaySendChar(&loop, 1500, 'n', simulate_input::CONTROL)));

  // Watch for new window
  const char* kNewWindowTitle = "Internet Explorer";
  EXPECT_CALL(mock, OnWindowDetected(_, testing::HasSubstr(kNewWindowTitle)))
      .WillOnce(testing::DoAll(
          DoCloseWindow(),
          CloseBrowserMock(&mock)));

  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kKeyEventUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// Test page reload with ctrl+R
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_CtrlR) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;

  // In sequence since we want to do different things on two loads
  // on the same mock for the same URLs
  ::testing::InSequence sequence;

  mock.ExpectNavigationAndSwitchSequence(kKeyEventUrl);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kKeyEventUrl)))
      .WillOnce(testing::DoAll(
          SetFocusToChrome(&mock),
          DelaySendChar(&loop, 1500, 'r', simulate_input::CONTROL)));

  // mock.ExpectNavigation(kKeyEventUrl);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kKeyEventUrl)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          CloseBrowserMock(&mock)));

  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kKeyEventUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// Test window close with ctrl+w
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_CtrlW) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;

  mock.ExpectNavigationAndSwitch(kKeyEventUrl);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kKeyEventUrl)))
      .WillOnce(testing::DoAll(
          SetFocusToChrome(&mock),
          DelaySendChar(&loop, 1500, 'w', simulate_input::CONTROL)));

  EXPECT_CALL(mock, OnQuit()).WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kKeyEventUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// Test address bar navigation with Alt+d and URL
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_AltD) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;
  ::testing::InSequence sequence;

  mock.ExpectNavigationAndSwitchSequence(kSubFrameUrl1);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(testing::DoAll(
          SetFocusToChrome(&mock),
          TypeUrlInAddressBar(&loop, kSubFrameUrl2, 1500)));

  mock.ExpectNavigationAndSwitchSequence(kSubFrameUrl2);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl2)))
      .WillOnce(CloseBrowserMock(&mock));

  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kSubFrameUrl1);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// This test launches chrome frame in full tab mode in IE by having IE navigate
// to a url. It then looks for the chrome renderer window and posts
// the WM_RBUTTONDOWN/WM_RBUTTONUP messages to it, which bring up the context
// menu. This is followed by keyboard messages sent via SendInput to select
// the About chrome frame menu option, which would then bring up a new window
// with the chrome revision. The test finally checks for success by comparing
// the URL of the window being opened with cf:about:version, which indicates
// that the operation succeeded.
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_AboutChromeFrame) {
  CloseIeAtEndOfScope last_resort_close_ie;

  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> new_window_mock;
  chrome_frame_test::TimedMsgLoop loop;

  mock.ExpectNavigationAndSwitch(kSubFrameUrl1);

  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop, 500, VK_UP, 1, simulate_input::NONE)));

  // Watch for new window
  mock.ExpectNewWindow(&new_window_mock);

  EXPECT_CALL(new_window_mock, OnLoad(testing::StrCaseEq(kAboutVersion)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrlWithGcf(&new_window_mock),
          CloseBrowserMock(&new_window_mock)));
  EXPECT_CALL(new_window_mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(CloseBrowserMock(&mock));
  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kSubFrameUrl1);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// Hack to pass a reference to the argument instead of value. Passing by
// value evaluates the argument at the mock specification time which is
// not always ideal. For e.g. At the time of mock creation, web_browser2_
// pointer is not set up yet so by passing a reference to it instead of
// a value we allow it to be created later.
template <typename T> T** ReceivePointer(scoped_refptr<T>& p) {  // NOLINT
  return reinterpret_cast<T**>(&p);
}

// Full tab mode back/forward test
// Launch and navigate chrome frame to a set of URLs and test back forward
TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_BackForward) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;
  ::testing::InSequence sequence;   // Everything in sequence

  mock.ExpectNavigationAndSwitchSequence(kSubFrameUrl1);

  // Navigate to url 2 after the previous navigation is complete.
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          Navigate(&mock, kSubFrameUrl2)));
  mock.ExpectNavigationAndSwitchSequence(kSubFrameUrl2);

  // Navigate to url 3 after the previous navigation is complete
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl2)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          Navigate(&mock, kSubFrameUrl3)));

  // We have reached url 3 and have two back entries for url 1 & 2
  // Go back to url 2 now
  mock.ExpectNavigationAndSwitchSequence(kSubFrameUrl3);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl3)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
              ReceivePointer(mock.web_browser2_), &IWebBrowser::GoBack)))));

  // We have reached url 2 and have 1 back & 1 forward entries for url 1 & 3
  // Go back to url 1 now
  mock.ExpectNavigation(kSubFrameUrl2);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl2)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
              ReceivePointer(mock.web_browser2_), &IWebBrowser::GoBack)))));

  // We have reached url 1 and have 0 back & 2 forward entries for url 2 & 3
  // Go back to url 1 now
  mock.ExpectNavigation(kSubFrameUrl1);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          CloseBrowserMock(&mock)));
  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kSubFrameUrl1);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

const wchar_t kAnchorUrl[] = L"http://localhost:1337/files/anchor.html";
const wchar_t kAnchor1Url[] = L"http://localhost:1337/files/anchor.html#a1";
const wchar_t kAnchor2Url[] = L"http://localhost:1337/files/anchor.html#a2";
const wchar_t kAnchor3Url[] = L"http://localhost:1337/files/anchor.html#a3";

// Full tab mode back/forward test
// Launch and navigate chrome frame to a set of URLs and test back forward
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_BackForwardAnchor) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;
  ::testing::InSequence sequence;   // Everything in sequence

  // Back/Forward state at this point:
  // Back: 0
  // Forward: 0
  mock.ExpectNavigationAndSwitchSequence(kAnchorUrl);

  // Navigate to anchor 1:
  // - First set focus to chrome renderer window
  //   Call WebBrowserEventSink::SetFocusToChrome only once
  //   in the beginning. Calling it again will change focus from the
  //   current location to an element near the simulated mouse.click.
  // - Then send keyboard input of TAB + ENTER to cause navigation.
  //   It's better to send input as PostDelayedTask since the Activex
  //   message loop on the other side might be blocked when we get
  //   called in Onload.
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchorUrl)))
      .WillOnce(testing::DoAll(
          SetFocusToChrome(&mock),
          DelaySendString(&loop, 200, tab_enter_keys)));

  mock.ExpectNavigationSequenceForAnchors(kAnchor1Url);
  // Navigate to anchor 2 after the previous navigation is complete
  // Back/Forward state at this point:
  // Back: 1 (kAnchorUrl)
  // Forward: 0
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchor1Url)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          DelaySendString(&loop, 200, tab_enter_keys)));
  mock.ExpectNavigationSequenceForAnchors(kAnchor2Url);

  // Navigate to anchor 3 after the previous navigation is complete
  // Back/Forward state at this point:
  // Back: 2 (kAnchorUrl, kAnchor1Url)
  // Forward: 0
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchor2Url)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          DelaySendString(&loop, 200, tab_enter_keys)));
  mock.ExpectNavigationSequenceForAnchors(kAnchor3Url);

  // We will reach anchor 3 once the navigation is complete,
  // then go back to anchor 2
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 0
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchor3Url)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
              ReceivePointer(mock.web_browser2_), &IWebBrowser::GoBack)))));
  mock.ExpectNavigationSequenceForAnchors(kAnchor2Url);

  // We will reach anchor 2 once the navigation is complete,
  // then go back to anchor 1
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 1 (kAnchor3Url)
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchor2Url)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
              ReceivePointer(mock.web_browser2_), &IWebBrowser::GoBack)))));
  mock.ExpectNavigationSequenceForAnchors(kAnchor1Url);

  // We will reach anchor 1 once the navigation is complete,
  // now go forward to anchor 2
  // Back/Forward state at this point:
  // Back: 2 (kAnchorUrl, kAnchor1Url)
  // Forward: 2 (kAnchor2Url, kAnchor3Url)
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchor1Url)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
              ReceivePointer(mock.web_browser2_), &IWebBrowser::GoForward)))));
  mock.ExpectNavigationSequenceForAnchors(kAnchor2Url);

  // We have reached anchor 2, go forward to anchor 3 again
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 1 (kAnchor3Url)
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchor2Url)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          testing::IgnoreResult(testing::InvokeWithoutArgs(CreateFunctor(
              ReceivePointer(mock.web_browser2_), &IWebBrowser::GoForward)))));
  mock.ExpectNavigationSequenceForAnchors(kAnchor3Url);

  // We have gone a few steps back and forward, this should be enough for now.
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchor3Url)))
      .WillOnce(CloseBrowserMock(&mock));
  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kAnchorUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// Full tab mode view source test
// Launch and navigate chrome frame and invoke view source functionality
// This test has been marked FLAKY
// http://code.google.com/p/chromium/issues/detail?id=35370
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_ViewSource) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> view_source_mock;
  chrome_frame_test::TimedMsgLoop loop;
  ::testing::InSequence sequence;   // Everything in sequence

  // After navigation invoke view soruce action using IWebBrowser2::ExecWB
  mock.ExpectNavigationAndSwitchSequence(kAnchorUrl);

  VARIANT empty = ScopedVariant::kEmptyVariant;
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchorUrl)))
      .WillOnce(testing::InvokeWithoutArgs(
          CreateFunctor(&mock, &MockWebBrowserEventSink::Exec, &CGID_MSHTML,
                        static_cast<OLECMDID>(IDM_VIEWSOURCE),
                        OLECMDEXECOPT_DONTPROMPTUSER, &empty, &empty)));

  // Expect notification for view-source window, handle new window event
  // and attach a new mock to the received web browser
  std::wstring view_source_url;
  view_source_url += UTF8ToWide(chrome::kViewSourceScheme);
  view_source_url += L":";
  view_source_url += kAnchorUrl;
  std::wstring url_in_new_window = kChromeProtocolPrefix;
  url_in_new_window += view_source_url;

  mock.ExpectNewWindow(&view_source_mock);
  EXPECT_CALL(view_source_mock, OnLoad(testing::StrCaseEq(view_source_url)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrlWithGcf(&view_source_mock),
          CloseBrowserMock(&view_source_mock)));

  EXPECT_CALL(view_source_mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(CloseBrowserMock(&mock));

  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kAnchorUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

const wchar_t kBeforeUnloadTest[] =
    L"http://localhost:1337/files/fulltab_before_unload_event_test.html";

const wchar_t kBeforeUnloadMain[] =
    L"http://localhost:1337/files/fulltab_before_unload_event_main.html";

TEST_F(ChromeFrameTestWithWebServer, FullTabModeIE_UnloadEventTest) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;
  ::testing::InSequence sequence;   // Everything in sequence

  mock.ExpectNavigationAndSwitchSequence(kBeforeUnloadTest);
  EXPECT_CALL(mock, OnLoad(_));

  mock.ExpectNavigationAndSwitchSequence(kBeforeUnloadMain);
  EXPECT_CALL(mock, OnLoad(_));

  EXPECT_CALL(mock, OnMessage(_, _, _))
      .WillOnce(CloseBrowserMock(&mock));
  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kBeforeUnloadTest);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// NOTE: This test is currently disabled as we haven't finished implementing
// support for this yet.  The test (as written) works fine for IE.  CF might
// have a different set of requirements once we fully support this and hence
// the test might need some refining before being enabled.
TEST_F(ChromeFrameTestWithWebServer,
       DISABLED_FullTabModeIE_DownloadInNewWindow) {
  CloseIeAtEndOfScope last_resort_close_ie;
  const wchar_t kDownloadFromNewWin[] =
      L"http://localhost:1337/files/full_tab_download_from_new_window.html";

  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;

  mock.ExpectNavigationAndSwitch(kDownloadFromNewWin);

  EXPECT_CALL(mock, OnNewWindow3(_, _, _, _, _));

  ComStackObjectWithUninitialize<MockWebBrowserEventSink> new_window_mock;
  EXPECT_CALL(mock, OnNewBrowserWindow(_, _))
      .WillOnce(testing::WithArgs<0>(testing::Invoke(CreateFunctor(
          &new_window_mock, &MockWebBrowserEventSink::Attach))));
  EXPECT_CALL(new_window_mock, OnBeforeNavigate2(_, _, _, _, _, _, _));

  EXPECT_CALL(new_window_mock, OnFileDownload(VARIANT_FALSE, _))
          .Times(2)
          .WillRepeatedly(CloseBrowserMock(&new_window_mock));

  EXPECT_CALL(new_window_mock, OnNavigateComplete2(_, _));

  EXPECT_CALL(new_window_mock, OnQuit()).WillOnce(CloseBrowserMock(&mock));
  EXPECT_CALL(mock, OnQuit()).WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kDownloadFromNewWin);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// Test Back/Forward from context menu. Loads page 1 in chrome and page 2
// in IE. Then it tests back and forward using context menu
// Disabling this test as it won't work as per the current chrome external tab
// design.
// http://code.google.com/p/chromium/issues/detail?id=46615
TEST_F(ChromeFrameTestWithWebServer,
       DISABLED_FullTabModeIE_ContextMenuBackForward) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;

  ::testing::InSequence sequence;   // Everything in sequence

  // Navigate to url 2 after the previous navigation is complete.
  mock.ExpectNavigationAndSwitchSequence(kSubFrameUrl1);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(Navigate(&mock, kSubFrameUrl2));

  mock.ExpectNavigationAndSwitchSequence(kSubFrameUrl2);

  // Go back using Rt-Click + DOWN + ENTER
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl2)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop, 500, VK_DOWN, 1, simulate_input::NONE)));
  mock.ExpectNavigation(kSubFrameUrl1);

  // Go forward using Rt-Click + DOWN + DOWN + ENTER
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop, 500, VK_DOWN, 2, simulate_input::NONE)));
  mock.ExpectNavigation(kSubFrameUrl2);

  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl2)))
      .WillOnce(CloseBrowserMock(&mock));
  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kSubFrameUrl1);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// Test Reload from context menu.
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_ContextMenuReload) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;

  ::testing::InSequence sequence;   // Everything in sequence

  mock.ExpectNavigationAndSwitchSequence(kSubFrameUrl1);

  // Reload using Rt-Click + DOWN + DOWN + DOWN + ENTER
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop, 500, VK_DOWN, 3, simulate_input::NONE)));

  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(CloseBrowserMock(&mock));
  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kSubFrameUrl1);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// Test view source using context menu
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer,
       FLAKY_FullTabModeIE_ContextMenuViewSource) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> view_source_mock;
  chrome_frame_test::TimedMsgLoop loop;
  ::testing::InSequence sequence;   // Everything in sequence

  // View source using Rt-Click + UP + UP + UP + UP + ENTER
  mock.ExpectNavigationAndSwitchSequence(kAnchorUrl);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchorUrl)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop, 500, VK_UP, 4, simulate_input::NONE)));

  // Expect notification for view-source window, handle new window event
  // and attach a new mock to the received web browser
  std::wstring view_source_url;
  view_source_url += UTF8ToWide(chrome::kViewSourceScheme);
  view_source_url += L":";
  view_source_url += kAnchorUrl;
  std::wstring url_in_new_window = kChromeProtocolPrefix;
  url_in_new_window += view_source_url;

  mock.ExpectNewWindow(&view_source_mock);
  EXPECT_CALL(view_source_mock, OnLoad(testing::StrCaseEq(view_source_url)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrlWithGcf(&view_source_mock),
          CloseBrowserMock(&view_source_mock)));
  EXPECT_CALL(view_source_mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(CloseBrowserMock(&mock));

  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kAnchorUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

TEST_F(ChromeFrameTestWithWebServer,
       FLAKY_FullTabModeIE_ContextMenuPageInfo) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;
  ::testing::InSequence sequence;   // Everything in sequence

  // View page information using Rt-Click + UP + UP + UP + ENTER
  const wchar_t* kPageInfoWindowClass = L"Chrome_WidgetWin_0";
  mock.ExpectNavigationAndSwitchSequence(kAnchorUrl);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchorUrl)))
      .WillOnce(testing::DoAll(
          WatchWindow(&mock, kPageInfoWindowClass),
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop, 500, VK_UP, 3, simulate_input::NONE)));

  // Expect page info dialog to pop up. Dismiss the dialog with 'Esc' key
  const char* kPageInfoCaption = "Security Information";
  EXPECT_CALL(mock, OnWindowDetected(_, testing::StrCaseEq(kPageInfoCaption)))
      .WillOnce(testing::DoAll(
          DelaySendChar(&loop, 100, VK_ESCAPE, simulate_input::NONE),
          DelayCloseBrowserMock(&loop, 2000, &mock)));

  EXPECT_CALL(mock, OnQuit()).WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kAnchorUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

TEST_F(ChromeFrameTestWithWebServer,
       FLAKY_FullTabModeIE_ContextMenuInspector) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;
  ::testing::InSequence sequence;   // Everything in sequence

  // Open developer tools using Rt-Click + UP + UP + ENTER
  const wchar_t* kPageInfoWindowClass = L"Chrome_WidgetWin_0";
  mock.ExpectNavigationAndSwitchSequence(kAnchorUrl);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchorUrl)))
      .WillOnce(testing::DoAll(
          WatchWindow(&mock, kPageInfoWindowClass),
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop, 500, VK_UP, 2, simulate_input::NONE)));

  // Devtools begins life with "Untitled" caption and it changes
  // later to the 'Developer Tools - <url> form.
  const char* kPageInfoCaption = "Untitled";
  EXPECT_CALL(mock, OnWindowDetected(_, testing::StartsWith(kPageInfoCaption)))
      .WillOnce(testing::DoAll(
          StopWindowWatching(&mock),
          SetFocusToChrome(&mock),
          DelayCloseBrowserMock(&loop, 2000, &mock)));

  EXPECT_CALL(mock, OnQuit()).WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kAnchorUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

TEST_F(ChromeFrameTestWithWebServer,
       FLAKY_FullTabModeIE_ContextMenuSaveAs) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;
  ::testing::InSequence sequence;   // Everything in sequence

  // Open'Save As' dialog using Rt-Click + DOWN + DOWN + DOWN + DOWN + ENTER
  const wchar_t* kSaveDlgClass = L"#32770";
  mock.ExpectNavigationAndSwitchSequence(kAnchorUrl);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchorUrl)))
      .WillOnce(testing::DoAll(
          WatchWindow(&mock, kSaveDlgClass),
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::RIGHT),
          SendExtendedKeysEnter(&loop, 500, VK_DOWN, 4, simulate_input::NONE)));

  FilePath temp_file_path;
  EXPECT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));
  temp_file_path = temp_file_path.ReplaceExtension(L".htm");

  const wchar_t* kSaveFileName = temp_file_path.value().c_str();
  DeleteFile(kSaveFileName);

  const char* kSaveDlgCaption = "Save As";
  EXPECT_CALL(mock, OnWindowDetected(_, testing::StrCaseEq(kSaveDlgCaption)))
      .WillOnce(testing::DoAll(
          StopWindowWatching(&mock),
          DelaySendString(&loop, 100, kSaveFileName),
          DelaySendChar(&loop, 200, VK_RETURN, simulate_input::NONE),
          DelayCloseBrowserMock(&loop, 4000, &mock)));

  EXPECT_CALL(mock, OnQuit()).WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kAnchorUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  ASSERT_NE(INVALID_FILE_ATTRIBUTES, GetFileAttributes(kSaveFileName));
  ASSERT_TRUE(DeleteFile(kSaveFileName));
}

// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer,
       FLAKY_FullTabModeIE_KeyboardBackForwardTest) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;

  ::testing::InSequence sequence;   // Everything in sequence

  // This test performs the following steps.
  // 1. Launches IE and navigates to
  //    http://localhost:1337/files/sub_frame1.html
  // 2. It then navigates to
  //    http://localhost:1337/files/sub_frame2.html
  // 3. Sends the VK_BACK keystroke to IE, which should navigate back to
  //    http://localhost:1337/files/sub_frame1.html
  // 4. Sends the Shift + VK_BACK keystroke to IE which should navigate
  //    forward to http://localhost:1337/files/sub_frame2.html
  mock.ExpectNavigationAndSwitchSequence(kSubFrameUrl1);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(Navigate(&mock, kSubFrameUrl2));

  mock.ExpectNavigationAndSwitchSequence(kSubFrameUrl2);

  short bkspace = VkKeyScanA(VK_BACK);  // NOLINT
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl2)))
      .WillOnce(testing::DoAll(
          SetFocusToChrome(&mock),
          DelaySendScanCode(&loop, 500, bkspace, simulate_input::NONE)));

  mock.ExpectNavigation(kSubFrameUrl1);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(testing::DoAll(
          SetFocusToChrome(&mock),
          DelaySendScanCode(&loop, 1500, bkspace, simulate_input::SHIFT)));

  mock.ExpectNavigation(kSubFrameUrl2);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl2)))
      .WillOnce(CloseBrowserMock(&mock));

  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kSubFrameUrl1);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// http://code.google.com/p/chromium/issues/detail?id=38566
TEST_F(ChromeFrameTestWithWebServer, DISABLED_FullTabModeIE_MenuSaveAs) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;
  ::testing::InSequence sequence;   // Everything in sequence

  // Open'Save As' dialog using Alt+F, a, note the small 'f'
  // in DelaySendChar (otherwise DelaySendChar will
  // OR in a shift.
  const wchar_t* kSaveDlgClass = L"#32770";
  mock.ExpectNavigationAndSwitchSequence(kKeyEventUrl);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kKeyEventUrl)))
      .WillOnce(testing::DoAll(
          WatchWindow(&mock, kSaveDlgClass),
          SetFocusToChrome(&mock),
          DelaySendChar(&loop, 1500, 'f', simulate_input::ALT),
          DelaySendChar(&loop, 2500, 'a', simulate_input::NONE)));

  FilePath temp_file_path;
  EXPECT_TRUE(file_util::CreateTemporaryFile(&temp_file_path));

  const wchar_t* kSaveFileName = temp_file_path.value().c_str();
  const char* kSaveDlgCaption = "Save As";
  EXPECT_CALL(mock, OnWindowDetected(_, testing::StrCaseEq(kSaveDlgCaption)))
      .WillOnce(testing::DoAll(
          StopWindowWatching(&mock),
          DelaySendString(&loop, 100, kSaveFileName),
          DelaySendChar(&loop, 200, VK_RETURN, simulate_input::NONE),
          DelayCloseBrowserMock(&loop, 4000, &mock)));

  EXPECT_CALL(mock, OnQuit()).WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kKeyEventUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  ASSERT_NE(INVALID_FILE_ATTRIBUTES, GetFileAttributes(kSaveFileName));
  ASSERT_TRUE(DeleteFile(kSaveFileName));
}

const wchar_t kHostBrowserUrl[] =
    L"http://localhost:1337/files/host_browser.html";

TEST_F(ChromeFrameTestWithWebServer,
       FLAKY_FullTabMode_SwitchFromIEToChromeFrame) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;

  EXPECT_CALL(mock, OnFileDownload(VARIANT_TRUE, _))
              .Times(testing::AnyNumber());

  ::testing::InSequence sequence;   // Everything in sequence

  // This test performs the following steps.
  // 1. Launches IE and navigates to
  //    http://localhost:1337/files/back_to_ie.html, which should render in IE.
  // 2. It then navigates to
  //    http://localhost:1337/files/sub_frame1.html which should render in
  //    ChromeFrame
  EXPECT_CALL(mock, OnBeforeNavigate2(_,
              testing::Field(&VARIANT::bstrVal,
              testing::StrCaseEq(kHostBrowserUrl)), _, _, _, _, _));

  // When we receive a navigate complete notification for the initial URL
  // initiate a navigation to a url which should be rendered in ChromeFrame.
  EXPECT_CALL(mock, OnNavigateComplete2(_,
              testing::Field(&VARIANT::bstrVal,
              testing::StrCaseEq(kHostBrowserUrl))))
      .Times(1)
      .WillOnce(TypeUrlInAddressBar(&loop, kSubFrameUrl1, 1500));

  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kHostBrowserUrl)))
      .Times(0);

  mock.ExpectNavigationAndSwitch(kSubFrameUrl1);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock),
          CloseBrowserMock(&mock)));

  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kHostBrowserUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds * 2);
}

// Test has been disabled as it causes the IE8 bot to hang at times.
// http://crbug.com/47596
TEST(IEPrivacy, DISABLED_NavigationToRestrictedSite) {
  if (IsIBrowserServicePatchEnabled()) {
    LOG(ERROR) << "Not running test. IBrowserServicePatch is in place.";
    return;
  }
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;
  ChromeFrameHTTPServer server;
  server.SetUp();

  ScopedComPtr<IInternetSecurityManager> security_manager;
  HRESULT hr = security_manager.CreateInstance(CLSID_InternetSecurityManager);
  ASSERT_HRESULT_SUCCEEDED(hr);
  // Add localhost to restricted sites zone.
  hr = security_manager->SetZoneMapping(URLZONE_UNTRUSTED,
      L"http://localhost:1337", SZM_CREATE);

  EXPECT_CALL(mock, OnFileDownload(_, _))
      .Times(testing::AnyNumber());

  ProtocolPatchMethod patch_method = GetPatchMethod();

  const wchar_t* url = L"http://localhost:1337/files/meta_tag.html";
  const wchar_t* kDialogClass = L"#32770";

  EXPECT_CALL(mock, OnBeforeNavigate2(_,
      testing::Field(&VARIANT::bstrVal,
      testing::StrCaseEq(url)), _, _, _, _, _))
      .Times(1)
      .WillOnce(WatchWindow(&mock, kDialogClass));

  if (patch_method == PATCH_METHOD_INET_PROTOCOL) {
    EXPECT_CALL(mock, OnBeforeNavigate2(_,
        testing::Field(&VARIANT::bstrVal,
        testing::HasSubstr(L"res://")), _, _, _, _, _))
        .Times(1);
  }

  EXPECT_CALL(mock, OnNavigateComplete2(_,
      testing::Field(&VARIANT::bstrVal, testing::StrCaseEq(url))))
      .Times(testing::AtMost(1));

  const char* kAlertDlgCaption = "Security Alert";
  EXPECT_CALL(mock, OnWindowDetected(_, testing::StrEq(kAlertDlgCaption)))
      .Times(1)
      .WillOnce(testing::DoAll(
          DoCloseWindow(),
          QUIT_LOOP_SOON(loop, 1)));

  EXPECT_CALL(mock, OnLoad(_)).Times(0);

  hr = mock.LaunchIEAndNavigate(url);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_OK) {
    ASSERT_TRUE(mock.web_browser2() != NULL);
    loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds * 2);
  }

  ASSERT_HRESULT_SUCCEEDED(security_manager->SetZoneMapping(URLZONE_UNTRUSTED,
      L"http://localhost:1337", SZM_DELETE));
}

// See bug 36694 for details.  http://crbug.com/36694
TEST_F(ChromeFrameTestWithWebServer,
       DISABLED_FullTabModeIE_TestDownloadFromForm) {
  if (IsIBrowserServicePatchEnabled()) {
    LOG(ERROR) << "Not running test. IBrowserServicePatch is in place.";
    return;
  }

  CloseIeAtEndOfScope last_resort_close_ie;

  // The content of our HTML test page.  This will be returned whenever
  // we reply to a GET request.
  static const char kHtml[] =
      "<html><head>\n"
      "<title>ChromeFrame Form Download Test</title>\n"
      // To see how this test runs with only IE (no CF in the picture), comment
      // out this meta tag.  The outcome of the test should be identical.
      "<meta http-equiv=\"X-UA-Compatible\" content=\"chrome=1\" />\n"
      "</head>\n"
      "<script language=\"javascript\">\n"
      "function SubmitForm() {\n"
      "  var form = document.forms['myform'];\n"
      "  form.action = document.location;\n"
      "  form.submit();\n"
      "  return true;\n"
      "}\n"
      "</script>\n"
      "<body onload=\"SubmitForm();\">\n"
      "<form method=\"post\" action=\"foo.html\" id=\"myform\">\n"
      "  <input type=\"hidden\" name=\"Field1\" value=\"myvalue\" />\n"
      "  <input type=\"button\" name=\"btn\" value=\"Test Download\" "
          "onclick=\"return SubmitForm();\" id=\"Button1\"/>\n"
      "</form></body></html>\n";

  // The content of our HTML test page.  This will be returned whenever
  // we reply to a POST request.
  static const char kText[] =
      "This is a text file (in case you were wondering).";

  // This http response class will return an HTML document that contains
  // a form whenever it receives a GET request.  Whenever it gets a POST
  // request, it will respond with a text file that needs to be downloaded
  // (content-disposition is "attachment").
  class CustomResponse : public test_server::ResponseForPath {
   public:
    explicit CustomResponse(const char* path)
      : test_server::ResponseForPath(path), is_post_(false),
        post_requests_(0), get_requests_(0) {
    }

    virtual bool GetContentType(std::string* content_type) const {
      DCHECK(!is_post_);
      return false;
    }

    virtual size_t ContentLength() const {
      DCHECK(!is_post_);
      return sizeof(kHtml) - 1;
    }

    virtual bool GetCustomHeaders(std::string* headers) const {
      if (!is_post_)
        return false;
      *headers = StringPrintf(
          "HTTP/1.1 200 OK\r\n"
          "Content-Disposition: attachment;filename=\"test.txt\"\r\n"
          "Content-Type: application/text\r\n"
          "Connection: close\r\n"
          "Content-Length: %i\r\n\r\n", sizeof(kText) - 1);
      return true;
    }

    virtual bool Matches(const test_server::Request& r) const {
      bool match = __super::Matches(r);
      if (match) {
        is_post_ = LowerCaseEqualsASCII(r.method().c_str(), "post");
      }
      return match;
    }

    virtual void WriteContents(ListenSocket* socket) const {
      if (is_post_) {
        socket->Send(kText, sizeof(kText) - 1, false);
      } else {
        socket->Send(kHtml, sizeof(kHtml) - 1, false);
      }
    }

    virtual void IncrementAccessCounter() {
      __super::IncrementAccessCounter();
      if (is_post_) {
        post_requests_++;
      } else {
        get_requests_++;
      }
    }

    size_t get_request_count() const {
      return get_requests_;
    }

    size_t post_request_count() const {
      return get_requests_;
    }

   protected:
    mutable bool is_post_;
    size_t post_requests_;
    size_t get_requests_;
  };

  chrome_frame_test::TimedMsgLoop loop;  // must come before the server.
  SimpleWebServerTest server(46664);
  CustomResponse* response = new CustomResponse("/form.html");
  server.web_server()->AddResponse(response);

  std::wstring url(server.FormatHttpPath(L"form.html"));

  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;

  // Will get called twice.  Once for HTML, once for the download.
  EXPECT_CALL(mock, OnBeforeNavigate2(_,
      testing::Field(&VARIANT::bstrVal,
      testing::StrCaseEq(url)), _, _, _, _, _)).Times(2);
  // Will get called twice in the case of CF, once if no CF.
  EXPECT_CALL(mock, OnFileDownload(VARIANT_TRUE, _))
      .Times(testing::AnyNumber());
  EXPECT_CALL(mock, OnNavigateComplete2(_,
      testing::Field(&VARIANT::bstrVal, testing::StrCaseEq(url))))
      .Times(testing::AnyNumber());
  EXPECT_CALL(mock, OnFileDownload(VARIANT_FALSE, _))
      .Times(testing::AnyNumber())
      .WillRepeatedly(CloseBrowserMock(&mock));

  EXPECT_CALL(mock, OnQuit()).WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(url);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_OK) {
    ASSERT_TRUE(mock.web_browser2() != NULL);
    loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds * 2);
  }

  EXPECT_EQ(1, response->get_request_count());
  EXPECT_EQ(1, response->post_request_count());
}

// This test validates that typing in URLs with a fragment in them switch to
// to ChromeFrame correctly.
TEST_F(ChromeFrameTestWithWebServer,
       FLAKY_FullTabModeIE_AltD_AnchorUrlNavigate) {
  if (IsIBrowserServicePatchEnabled()) {
    LOG(ERROR) << "Not running test. IBrowserServicePatch is in place.";
    return;
  }

  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  chrome_frame_test::TimedMsgLoop loop;
  ::testing::InSequence sequence;

  mock.ExpectNavigationAndSwitchSequence(kSubFrameUrl1);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kSubFrameUrl1)))
      .WillOnce(testing::DoAll(
          SetFocusToChrome(&mock),
          TypeUrlInAddressBar(&loop, kAnchor1Url, 1500)));

  mock.ExpectNavigationAndSwitchSequence(kAnchor1Url);
  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kAnchor1Url)))
      .WillOnce(CloseBrowserMock(&mock));

  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kSubFrameUrl1);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

// This test checks if window.open calls issued by a full tab mode ChromeFrame
// instance make it back to IE and then transitions back to Chrome as the
// window.open target page is supposed to render within Chrome and whether this
// window can be closed correctly.
// Marking this test as FLAKY initially as it relies on getting focus and user
// input which don't work correctly at times.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_WindowCloseInChrome) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> new_window_mock;
  chrome_frame_test::TimedMsgLoop loop;

  mock.ExpectNavigationAndSwitch(kWindowOpenUrl);

  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kWindowOpenUrl)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::LEFT),
          DelaySendChar(&loop, 500, 'O', simulate_input::NONE)));
  // Watch for new window
  mock.ExpectNewWindow(&new_window_mock);

  EXPECT_CALL(new_window_mock, OnLoad(testing::StrCaseEq(kWindowOpenPopupUrl)))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&new_window_mock),
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::LEFT),
          DelaySendChar(&loop, 500, 'C', simulate_input::NONE)));

  EXPECT_CALL(new_window_mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(CloseBrowserMock(&mock));

  EXPECT_CALL(mock, OnQuit())
      .Times(testing::AtMost(1))
      .WillOnce(QUIT_LOOP_SOON(loop, 2));

  HRESULT hr = mock.LaunchIEAndNavigate(kWindowOpenUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
}

const wchar_t kWindowOpenTargetBlankUrl[] =
    L"http://localhost:1337/files/chrome_frame_target_blank.html";

// This test checks if window.open calls with target blank issued for a
// different domain make it back to IE instead of completing the navigation
// within Chrome. We validate this by initiating a navigation to a non existent
// url which ensures we would get an error during navigation.
// Marking this test as FLAKY initially as it relies on getting focus and user
// input which don't work correctly at times.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer,
       FLAKY_FullTabModeIE_WindowOpenTargetBlankInChrome) {
  CloseIeAtEndOfScope last_resort_close_ie;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> mock;
  ComStackObjectWithUninitialize<MockWebBrowserEventSink> new_window_mock;
  chrome_frame_test::TimedMsgLoop loop;

  mock.ExpectNavigationAndSwitch(kWindowOpenTargetBlankUrl);

  EXPECT_CALL(mock, OnLoad(testing::StrCaseEq(kWindowOpenTargetBlankUrl)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClick(&mock, &loop, 0, 10, 10, simulate_input::LEFT),
          DelaySendChar(&loop, 500, 'O', simulate_input::NONE)));
  // Watch for new window
  mock.ExpectNewWindow(&new_window_mock);

  EXPECT_CALL(new_window_mock, OnBeforeNavigate2(_,
      testing::Field(&VARIANT::bstrVal,
      testing::HasSubstr(L"attach_external_tab")), _, _, _, _, _))
      .Times(testing::AtMost(1));

  EXPECT_CALL(new_window_mock, OnBeforeNavigate2(_,
      testing::Field(&VARIANT::bstrVal,
      testing::StrCaseEq(kHostBrowserUrl)), _, _, _, _, _))
      .Times(testing::AtMost(1));

  EXPECT_CALL(new_window_mock, OnNavigateError(_, _, _, _, _))
      .Times(1)
      .WillOnce(CloseBrowserMock(&new_window_mock));

  EXPECT_CALL(new_window_mock, OnQuit())
      .Times(1)
      .WillOnce(CloseBrowserMock(&mock));

  EXPECT_CALL(mock, OnQuit())
      .Times(1)
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kWindowOpenTargetBlankUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds * 2);

  ASSERT_TRUE(new_window_mock.web_browser2() != NULL);
}

