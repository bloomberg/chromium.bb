// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome_frame/test/test_mock_with_web_server.h"

#include <mshtmcid.h>

#include "chrome/common/url_constants.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/test/test_with_web_server.h"

#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::CreateFunctor;
using testing::_;

const int kChromeFrameLaunchDelay = 5;
const int kChromeFrameLongNavigationTimeoutInSeconds = 10;

const wchar_t kChromeFrameFileUrl[] = L"gcf:file:///C:/";

TEST(ChromeFrameTest, FullTabModeIE_DisallowedUrls) {
  chrome_frame_test::TimedMsgLoop loop;
  // If a navigation fails then IE issues a navigation to an interstitial
  // page. Catch this to track navigation errors as the NavigateError
  // notification does not seem to fire reliably.
  CComObjectStackEx<MockWebBrowserEventSink> mock;

  EXPECT_CALL(mock,
              OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                testing::StrCaseEq(kChromeFrameFileUrl)),
                                _, _, _, _, _))
      .Times(1)
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock,
              OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                testing::StartsWith(L"res:")),
                                _, _, _, _, _))
      .Times(1)
      .WillOnce(testing::DoAll(
          QUIT_LOOP(loop),
          testing::Return(S_OK)));

  HRESULT hr = mock.LaunchIEAndNavigate(kChromeFrameFileUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
}

const wchar_t kChromeFrameFullTabWindowOpenTestUrl[] =
    L"http://localhost:1337/files/chrome_frame_window_open.html";

const wchar_t kChromeFrameFullTabWindowOpenPopupUrl[] =
    L"http://localhost:1337/files/chrome_frame_window_open_popup.html";

// This test checks if window.open calls issued by a full tab mode ChromeFrame
// instance make it back to IE and then transitions back to Chrome as the
// window.open target page is supposed to render within Chrome.
// Marking this test as FLAKY initially as it relies on getting focus and user
// input which don't work correctly at times.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_WindowOpenInChrome) {
  chrome_frame_test::TimedMsgLoop loop;
  CComObjectStackEx<MockWebBrowserEventSink> mock;

  // NOTE:
  // Intentionally not in sequence since we have just one navigation
  // per mock, it's OK to be not in sequence as long as all the expections
  // are satisfied. Moreover, since the second mock expects a new window,
  // its events happen in random order.
  EXPECT_CALL(mock,
              OnBeforeNavigate2(
                  _, testing::Field(&VARIANT::bstrVal,
                  testing::StrCaseEq(kChromeFrameFullTabWindowOpenTestUrl)),
                  _, _, _, _, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return());

  EXPECT_CALL(mock,
              OnLoad(testing::StrEq(kChromeFrameFullTabWindowOpenTestUrl)))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&mock,
              &chrome_frame_test::WebBrowserEventSink::SetFocusToChrome)),
          testing::InvokeWithoutArgs(CreateFunctor(&loop,
              &chrome_frame_test::TimedMsgLoop::PostDelayedTask, FROM_HERE,
              NewRunnableMethod(
                  &mock,
                  &chrome_frame_test::WebBrowserEventSink::SendInputToChrome,
                  std::string("A")), 0))));

  // Watch for new window
  CComObjectStackEx<MockWebBrowserEventSink> new_window_mock;
  // Can't really check URL here since it will be of the form gcf:attach...
  EXPECT_CALL(mock, OnNewWindow3(_, _, _, _, _));
  EXPECT_CALL(mock, OnNewBrowserWindow(_, _))
    .WillOnce(testing::WithArgs<0>(
          testing::Invoke(CreateFunctor(&new_window_mock,
                                        &MockWebBrowserEventSink::Attach))));

  // Expect navigations on the new mock
  EXPECT_CALL(new_window_mock, OnBeforeNavigate2(_, _, _, _, _, _, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(S_OK));
  EXPECT_CALL(new_window_mock, OnNavigateComplete2(_, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return());
  EXPECT_CALL(new_window_mock,
              OnLoad(testing::StrEq(kChromeFrameFullTabWindowOpenPopupUrl)))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&mock,
              &MockWebBrowserEventSink::Uninitialize)),
          testing::InvokeWithoutArgs(CreateFunctor(&new_window_mock,
              &MockWebBrowserEventSink::Uninitialize)),
          testing::IgnoreResult(testing::InvokeWithoutArgs(
              &chrome_frame_test::CloseAllIEWindows)),
          QUIT_LOOP_SOON(loop, 2)));

  HRESULT hr = mock.LaunchIEAndNavigate(kChromeFrameFullTabWindowOpenTestUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
  ASSERT_TRUE(CheckResultFile(L"ChromeFrameWindowOpenPopup", "OK"));
}

const wchar_t kSubFrameUrl1[] =
    L"http://localhost:1337/files/sub_frame1.html";

const wchar_t kChromeFrameAboutVersion[] =
    L"gcf:about:version";

// This test launches chrome frame in full tab mode in IE by having IE navigate
// to gcf:about:blank. It then looks for the chrome renderer window and posts
// the WM_RBUTTONDOWN/WM_RBUTTONUP messages to it, which bring up the context
// menu. This is followed by keyboard messages sent via SendInput to select
// the About chrome frame menu option, which would then bring up a new window
// with the chrome revision. The test finally checks for success by comparing
// the URL of the window being opened with cf:about:version, which indicates
// that the operation succeeded.
// Marking this test FLAKY as it fails at times on the buildbot.
// http://code.google.com/p/chromium/issues/detail?id=26549
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_AboutChromeFrame) {
  chrome_frame_test::TimedMsgLoop loop;
  CComObjectStackEx<MockWebBrowserEventSink> mock;

  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl1)),
                                _, _, _, _, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return());

  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl1)))
      .WillOnce(testing::InvokeWithoutArgs(
          &chrome_frame_test::ShowChromeFrameContextMenu));

  EXPECT_CALL(mock,
              OnNewWindow3(_, _, _, _,
                           testing::StrCaseEq(kChromeFrameAboutVersion)))
      .WillOnce(QUIT_LOOP(loop));

  HRESULT hr = mock.LaunchIEAndNavigate(kSubFrameUrl1);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);

  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);

  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
}

const wchar_t kSubFrameUrl2[] =
    L"http://localhost:1337/files/sub_frame2.html";
const wchar_t kSubFrameUrl3[] =
    L"http://localhost:1337/files/sub_frame3.html";

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
  chrome_frame_test::TimedMsgLoop loop;
  CComObjectStackEx<MockWebBrowserEventSink> mock;
  ::testing::InSequence sequence;   // Everything in sequence

  // When the onhttpequiv patch is enabled, we will get two
  // BeforeNavigate2/OnNavigateComplete2 notifications due to
  // switching from IE to CF.
  // Note that when going backwards, we don't expect that since the extra
  // navigational entries in the travel log should have been removed.

  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl1)),
                                _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl1)),
                                _, _, _, _, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return());

  // Navigate to url 2 after the previous navigation is complete.
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl1)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(
              &mock, &chrome_frame_test::WebBrowserEventSink::Navigate,
              std::wstring(kSubFrameUrl2)))));

  // Expect BeforeNavigate/NavigateComplete twice here as well.
  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl2)),
                                _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl2)),
                                _, _, _, _, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return());

  // Navigate to url 3 after the previous navigation is complete
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl2)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(
              &mock, &chrome_frame_test::WebBrowserEventSink::Navigate,
              std::wstring(kSubFrameUrl3)))));

  // We have reached url 3 and have two back entries for url 1 & 2
  // Go back to url 2 now
  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl3)),
                                _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl3)),
                                _, _, _, _, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return());

  // Go back.
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl3)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoBack))));

  // We have reached url 2 and have 1 back & 1 forward entries for url 1 & 3
  // Go back to url 1 now
  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl2)),
                                _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl2)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoBack))));

  // We have reached url 1 and have 0 back & 2 forward entries for url 2 & 3
  // Go back to url 1 now
  EXPECT_CALL(mock,
      OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                          testing::StrCaseEq(kSubFrameUrl1)),
                                _, _, _, _, _));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(mock, OnLoad(testing::StrEq(kSubFrameUrl1)))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&mock,
              &chrome_frame_test::WebBrowserEventSink::Uninitialize)),
          testing::IgnoreResult(testing::InvokeWithoutArgs(
              &chrome_frame_test::CloseAllIEWindows)),
          QUIT_LOOP_SOON(loop, 2)));

  HRESULT hr = mock.LaunchIEAndNavigate(kSubFrameUrl1);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
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
  const char tab_enter_keystrokes[] = { VK_TAB, VK_RETURN, 0 };
  static const std::string tab_enter(tab_enter_keystrokes);
  chrome_frame_test::TimedMsgLoop loop;
  CComObjectStackEx<MockWebBrowserEventSink> mock;
  ::testing::InSequence sequence;   // Everything in sequence

  // When the onhttpequiv patch is enabled, we will get two
  // BeforeNavigate2/OnNavigateComplete2 notifications due to
  // switching from IE to CF.
  // Note that when going backwards, we don't expect that since the extra
  // navigational entries in the travel log should have been removed.
  // Same for navigating to anchors within a page that's already loaded.

  // Back/Forward state at this point:
  // Back: 0
  // Forward: 0
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchorUrl)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchorUrl)),
                                      _, _, _, _, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return());

  // Navigate to anchor 1:
  // - First set focus to chrome renderer window
  //   Call WebBrowserEventSink::SetFocusToChrome only once
  //   in the beginning. Calling it again will change focus from the
  //   current location to an element near the simulated mouse.click.
  // - Then send keyboard input of TAB + ENTER to cause navigation.
  //   It's better to send input as PostDelayedTask since the Activex
  //   message loop on the other side might be blocked when we get
  //   called in Onload.
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchorUrl)))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&mock,
              &chrome_frame_test::WebBrowserEventSink::SetFocusToChrome)),
          testing::InvokeWithoutArgs(CreateFunctor(&loop,
              &chrome_frame_test::TimedMsgLoop::PostDelayedTask, FROM_HERE,
              NewRunnableMethod(
                  &mock,
                  &chrome_frame_test::WebBrowserEventSink::SendInputToChrome,
                  std::string(tab_enter)), 0))));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor1Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // Navigate to anchor 2 after the previous navigation is complete
  // Back/Forward state at this point:
  // Back: 1 (kAnchorUrl)
  // Forward: 0
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor1Url)))
      .WillOnce(testing::InvokeWithoutArgs(
          CreateFunctor(
              &loop, &chrome_frame_test::TimedMsgLoop::PostDelayedTask,
              FROM_HERE,
              NewRunnableMethod(
                  &mock,
                  &chrome_frame_test::WebBrowserEventSink::SendInputToChrome,
                  std::string(tab_enter)), 0)));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor2Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // Navigate to anchor 3 after the previous navigation is complete
  // Back/Forward state at this point:
  // Back: 2 (kAnchorUrl, kAnchor1Url)
  // Forward: 0
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor2Url)))
      .WillOnce(testing::InvokeWithoutArgs(
          CreateFunctor(
              &loop, &chrome_frame_test::TimedMsgLoop::PostDelayedTask,
              FROM_HERE,
              NewRunnableMethod(
                  &mock,
                  &chrome_frame_test::WebBrowserEventSink::SendInputToChrome,
                  std::string(tab_enter)), 0)));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor3Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // We will reach anchor 3 once the navigation is complete,
  // then go back to anchor 2
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 0
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor3Url)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoBack))));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor2Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // We will reach anchor 2 once the navigation is complete,
  // then go back to anchor 1
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 1 (kAnchor3Url)
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor2Url)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoBack))));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor1Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // We will reach anchor 1 once the navigation is complete,
  // now go forward to anchor 2
  // Back/Forward state at this point:
  // Back: 2 (kAnchorUrl, kAnchor1Url)
  // Forward: 2 (kAnchor2Url, kAnchor3Url)
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor1Url)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoForward))));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor2Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // We have reached anchor 2, go forward to anchor 3 again
  // Back/Forward state at this point:
  // Back: 3 (kAnchorUrl, kAnchor1Url, kAnchor2Url)
  // Forward: 1 (kAnchor3Url)
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor2Url)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser::GoForward))));
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchor3Url)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  // We have gone a few steps back and forward, this should be enough for now.
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchor3Url)))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&mock,
              &chrome_frame_test::WebBrowserEventSink::Uninitialize)),
          testing::IgnoreResult(testing::InvokeWithoutArgs(
              &chrome_frame_test::CloseAllIEWindows)),
          QUIT_LOOP_SOON(loop, 2)));

  HRESULT hr = mock.LaunchIEAndNavigate(kAnchorUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
}

// Full tab mode view source test
// Launch and navigate chrome frame and invoke view source functionality
// This test has been marked FLAKY
// http://code.google.com/p/chromium/issues/detail?id=35370
TEST_F(ChromeFrameTestWithWebServer, FLAKY_FullTabModeIE_ViewSource) {
  chrome_frame_test::TimedMsgLoop loop;
  CComObjectStackEx<MockWebBrowserEventSink> mock;
  CComObjectStackEx<MockWebBrowserEventSink> view_source_mock;
  ::testing::InSequence sequence;   // Everything in sequence

  // After navigation invoke view soruce action using IWebBrowser2::ExecWB
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchorUrl)),
                                      _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock, OnBeforeNavigate2(_, testing::Field(&VARIANT::bstrVal,
                                      testing::StrCaseEq(kAnchorUrl)),
                                      _, _, _, _, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(S_OK));
  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return());

  VARIANT empty = ScopedVariant::kEmptyVariant;
  EXPECT_CALL(mock, OnLoad(testing::StrEq(kAnchorUrl)))
      .WillOnce(testing::IgnoreResult(testing::InvokeWithoutArgs(
          CreateFunctor(ReceivePointer(mock.web_browser2_),
                        &IWebBrowser2::ExecWB,
                        static_cast<OLECMDID>(IDM_VIEWSOURCE),
                        OLECMDEXECOPT_DONTPROMPTUSER, &empty, &empty))));

  // Expect notification for view-source window, handle new window event
  // and attach a new mock to the received web browser
  std::wstring view_source_url;
  view_source_url += UTF8ToWide(chrome::kViewSourceScheme);
  view_source_url += L":";
  view_source_url += kAnchorUrl;
  std::wstring url_in_new_window = kChromeProtocolPrefix;
  url_in_new_window += view_source_url;

  EXPECT_CALL(mock, OnNewWindow3(_, _, _, _,
                                 testing::StrCaseEq(url_in_new_window)));
  EXPECT_CALL(mock, OnNewBrowserWindow(_, _))
    .WillOnce(testing::WithArgs<0>(
          testing::Invoke(CreateFunctor(&view_source_mock,
                                        &MockWebBrowserEventSink::Attach))));

  // Expect navigations on the new mock
  EXPECT_CALL(view_source_mock, OnBeforeNavigate2(_,
              testing::Field(&VARIANT::bstrVal,
              testing::StrCaseEq(url_in_new_window)), _, _, _, _, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(S_OK));
  EXPECT_CALL(view_source_mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());
  EXPECT_CALL(view_source_mock, OnLoad(testing::StrEq(view_source_url)))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&mock,
              &MockWebBrowserEventSink::Uninitialize)),
          testing::InvokeWithoutArgs(CreateFunctor(&view_source_mock,
              &MockWebBrowserEventSink::Uninitialize)),
          testing::IgnoreResult(testing::InvokeWithoutArgs(
              &chrome_frame_test::CloseAllIEWindows)),
          QUIT_LOOP_SOON(loop, 2)));

  HRESULT hr = mock.LaunchIEAndNavigate(kAnchorUrl);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
}

const wchar_t kChromeFrameFullTabModeBeforeUnloadEventTest[] =
    L"http://localhost:1337/files/fulltab_before_unload_event_test.html";

const wchar_t kChromeFrameFullTabModeBeforeUnloadEventMain[] =
    L"http://localhost:1337/files/fulltab_before_unload_event_main.html";

TEST_F(ChromeFrameTestWithWebServer,
       FullTabModeIE_ChromeFrameUnloadEventTest) {
  chrome_frame_test::TimedMsgLoop loop;
  CComObjectStackEx<MockWebBrowserEventSink> mock;
  ::testing::InSequence sequence;   // Everything in sequence

  // We will get two BeforeNavigate2/OnNavigateComplete2 notifications due to
  // switching from IE to CF.
  EXPECT_CALL(
      mock,
      OnBeforeNavigate2(
          _, testing::Field(&VARIANT::bstrVal,
          testing::StrCaseEq(kChromeFrameFullTabModeBeforeUnloadEventTest)),
          _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(
      mock,
      OnBeforeNavigate2(
          _, testing::Field(&VARIANT::bstrVal,
          testing::StrCaseEq(kChromeFrameFullTabModeBeforeUnloadEventTest)),
          _, _, _, _, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return());

  EXPECT_CALL(mock, OnLoad(_)).WillOnce(testing::Return());

  // We will get two BeforeNavigate2/OnNavigateComplete2 notifications due to
  // switching from IE to CF.
  EXPECT_CALL(
      mock,
      OnBeforeNavigate2(
          _, testing::Field(&VARIANT::bstrVal,
          testing::StrCaseEq(kChromeFrameFullTabModeBeforeUnloadEventMain)),
          _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .WillOnce(testing::Return());

  EXPECT_CALL(
      mock,
      OnBeforeNavigate2(
          _, testing::Field(&VARIANT::bstrVal,
          testing::StrCaseEq(kChromeFrameFullTabModeBeforeUnloadEventMain)),
          _, _, _, _, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return(S_OK));

  EXPECT_CALL(mock, OnNavigateComplete2(_, _))
      .Times(testing::AnyNumber()).WillRepeatedly(testing::Return());

  EXPECT_CALL(mock, OnLoad(_)).WillOnce(testing::Return());

  EXPECT_CALL(mock, OnMessage(_))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&mock,
              &chrome_frame_test::WebBrowserEventSink::Uninitialize)),
          testing::IgnoreResult(testing::InvokeWithoutArgs(
              &chrome_frame_test::CloseAllIEWindows)),
          QUIT_LOOP_SOON(loop, 2)));

  HRESULT hr =
      mock.LaunchIEAndNavigate(kChromeFrameFullTabModeBeforeUnloadEventTest);
  ASSERT_HRESULT_SUCCEEDED(hr);
  if (hr == S_FALSE)
    return;

  ASSERT_TRUE(mock.web_browser2() != NULL);
  loop.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  mock.Uninitialize();
  chrome_frame_test::CloseAllIEWindows();
}

