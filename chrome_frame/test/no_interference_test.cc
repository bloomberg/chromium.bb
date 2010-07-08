// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/test_mock_with_web_server.h"

#include "base/scoped_variant_win.h"
#include "base/utf_string_conversions.h"
#include "chrome_frame/test/simulate_input.h"
#include "chrome_frame/test/test_with_web_server.h"

using chrome_frame_test::CloseIeAtEndOfScope;
using chrome_frame_test::ComStackObjectWithUninitialize;
using chrome_frame_test::kChromeFrameLongNavigationTimeoutInSeconds;
using testing::_;

namespace {

// The same as MockWebBrowserEventSink, except OnDocumentComplete is added.
// This class can be merged with its base class, but it would require some
// modification to most of the test cases.
class MockWebBrowserEventSink
    : public chrome_frame_test::MockWebBrowserEventSink {
 public:
  // This method is called for each OnDocumentComplete event in which the
  // document is rendered by IE and not CF.
  MOCK_METHOD1(OnIELoad, void (const wchar_t* url));  // NOLINT

  // The OnDocumentComplete event is received for every document load,
  // regardless of whether IE or CF is hosting. This method will call
  // OnIELoad iff the document was loaded in an IE renderer.
  STDMETHOD_(void, OnDocumentComplete)(IDispatch* dispatch,
                                       VARIANT* url_variant) {
    ScopedVariant url;
    if (url_variant)
      url.Reset(*url_variant);
    // To determine if the document is loaded in IE or CF, check if
    // IHTMLDocument2 can be retrieved. If so, call OnIELoad.
    ScopedComPtr<IWebBrowser2> web_browser2;
    web_browser2.QueryFrom(dispatch);
    EXPECT_TRUE(web_browser2);
    if (web_browser2) {
      ScopedComPtr<IDispatch> doc_dispatch;
      HRESULT hr = web_browser2->get_Document(doc_dispatch.Receive());
      EXPECT_HRESULT_SUCCEEDED(hr);
      EXPECT_TRUE(doc_dispatch);
      if (doc_dispatch) {
        ScopedComPtr<IHTMLDocument2> doc;
        doc.QueryFrom(doc_dispatch);
        if (doc) {
          OnIELoad(V_BSTR(&url));
        }
      }
    }
  }
};

// Fixture for tests which ensure that Chrome Frame does not interfere with
// normal IE operation.
// TODO(kkania): Move this fixture to test_mock_with_web_server.h and change
// those tests to use this too.
class NoInterferenceTest : public ChromeFrameTestWithWebServer {
 protected:
  // Launches IE and navigates to |url|, then waits until receiving a quit
  // message or the timeout is exceeded.
  void LaunchIEAndNavigate(const std::wstring& url) {
    EXPECT_CALL(mock_, OnQuit())
        .Times(testing::AtMost(1))
        .WillOnce(QUIT_LOOP(loop_));
    HRESULT hr = mock_.LaunchIEAndNavigate(url);
    ASSERT_HRESULT_SUCCEEDED(hr);
    if (hr == S_FALSE)
      return;

    ASSERT_TRUE(mock_.web_browser2() != NULL);
    loop_.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
    mock_.Uninitialize();
  }

  const std::wstring GetTestUrl(const wchar_t* relative_path) {
    std::wstring path = std::wstring(L"files/no_interference/") +
        relative_path;
    return UTF8ToWide(server_.Resolve(path.c_str()).spec());
  }

  const std::wstring empty_page_url() {
    return GetTestUrl(L"empty.html");
  }

  // Returns the url for a page with a single link, which points to the
  // empty page.
  const std::wstring link_page_url() {
    return GetTestUrl(L"link.html");
  }

  CloseIeAtEndOfScope last_resort_close_ie;
  chrome_frame_test::TimedMsgLoop loop_;
  CComObjectStackEx<testing::StrictMock<MockWebBrowserEventSink> > mock_;
};

ACTION_P(ExpectIERendererWindowHasFocus, mock) {
  mock->ExpectIERendererWindowHasFocus();
}

ACTION_P6(DelaySendMouseClickToIE, mock, loop, delay, x, y, button) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableMethod(mock,
      &MockWebBrowserEventSink::SendMouseClickToIE, x, y, button), delay);
}

ACTION_P2(OpenContextMenu, loop, delay) {
  loop->PostDelayedTask(FROM_HERE, NewRunnableFunction(
      simulate_input::SendScanCode, VK_F10, simulate_input::SHIFT), delay);
}

ACTION_P3(SelectItem, loop, delay, index) {
  chrome_frame_test::DelaySendExtendedKeysEnter(loop, delay, VK_DOWN, index + 1,
      simulate_input::NONE);
}

// A new IE renderer window should have focus.
TEST_F(NoInterferenceTest, FLAKY_SimpleFocus) {
  mock_.ExpectNavigationInIE(empty_page_url());
  EXPECT_CALL(mock_, OnIELoad(testing::StrCaseEq(empty_page_url())))
      .WillOnce(testing::DoAll(
          ExpectIERendererWindowHasFocus(&mock_),
          VerifyAddressBarUrl(&mock_),
          CloseBrowserMock(&mock_)));

  LaunchIEAndNavigate(empty_page_url());
}

// Javascript window.open should open a new window with an IE renderer.
TEST_F(NoInterferenceTest, FLAKY_JavascriptWindowOpen) {
  const std::wstring kWindowOpenUrl = GetTestUrl(L"window_open.html");
  ComStackObjectWithUninitialize<
      testing::StrictMock<MockWebBrowserEventSink> > new_window_mock;

  mock_.ExpectNavigationInIE(kWindowOpenUrl);
  EXPECT_CALL(mock_, OnIELoad(testing::StrCaseEq(kWindowOpenUrl)))
      .WillOnce(testing::DoAll(
          DelaySendMouseClickToIE(&mock_, &loop_, 0, 100, 100,
                                  simulate_input::LEFT),
          DelaySendMouseClickToIE(&mock_, &loop_, 0, 100, 100,
                                  simulate_input::LEFT)));

  EXPECT_CALL(mock_, OnNewWindow3(_, _, _, _, _));

  EXPECT_CALL(mock_, OnNewBrowserWindow(_, _))
      .WillOnce(testing::WithArgs<0>(testing::Invoke(CreateFunctor(
          &new_window_mock, &MockWebBrowserEventSink::Attach))));

  EXPECT_CALL(new_window_mock, OnBeforeNavigate2(_,
                  testing::Field(&VARIANT::bstrVal,
                  testing::StrCaseEq(empty_page_url())),
                  _, _, _, _, _));

  EXPECT_CALL(new_window_mock, OnFileDownload(VARIANT_TRUE, _))
      .Times(testing::AnyNumber());

  EXPECT_CALL(new_window_mock, OnNavigateComplete2(_,
                            testing::Field(&VARIANT::bstrVal,
                            testing::StrCaseEq(empty_page_url()))))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(CreateFunctor(&new_window_mock,
              &chrome_frame_test::MockWebBrowserEventSink::ExpectAddressBarUrl,
              empty_page_url())),
          DelayCloseBrowserMock(&loop_, 2000, &new_window_mock)));

  // The DocumentComplete at times fires twice for new windows. Hack to account
  // for that.
  EXPECT_CALL(new_window_mock,
              OnIELoad(testing::StrCaseEq(empty_page_url())))
      .Times(testing::AtMost(2));

  EXPECT_CALL(new_window_mock, OnQuit())
      .WillOnce(DelayCloseBrowserMock(&loop_, 2000, &mock_));

  LaunchIEAndNavigate(kWindowOpenUrl);
}

// Redirecting with window.location in Javascript should work.
TEST_F(NoInterferenceTest, JavascriptRedirect) {
  const std::wstring kRedirectUrl = GetTestUrl(L"javascript_redirect.html");
  mock_.ExpectNavigationInIE(kRedirectUrl);
  EXPECT_CALL(mock_, OnIELoad(testing::StrCaseEq(kRedirectUrl)))
      .WillOnce(VerifyAddressBarUrl(&mock_));

  mock_.ExpectNavigationInIE(empty_page_url());
  EXPECT_CALL(mock_, OnIELoad(testing::StrCaseEq(empty_page_url())))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock_),
          CloseBrowserMock(&mock_)));

  LaunchIEAndNavigate(kRedirectUrl);
}

TEST_F(NoInterferenceTest, FLAKY_FollowLink) {
  mock_.ExpectNavigationInIE(link_page_url());
  EXPECT_CALL(mock_, OnIELoad(testing::StrCaseEq(link_page_url())))
      .WillOnce(testing::DoAll(
          DelaySendMouseClickToIE(&mock_, &loop_, 0, 1, 1,
                                  simulate_input::LEFT),
          DelaySendScanCode(&loop_, 500, VK_TAB, simulate_input::NONE),
          DelaySendScanCode(&loop_, 1000, VK_RETURN, simulate_input::NONE)));

  mock_.ExpectNavigationInIE(empty_page_url());
  EXPECT_CALL(mock_, OnIELoad(testing::StrCaseEq(empty_page_url())))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock_),
          CloseBrowserMock(&mock_)));

  LaunchIEAndNavigate(link_page_url());
}

TEST_F(NoInterferenceTest, FLAKY_SelectContextMenuOpen) {
  mock_.ExpectNavigationInIE(link_page_url());
  // Focus the renderer window by clicking and then tab once to highlight the
  // link.
  EXPECT_CALL(mock_, OnIELoad(testing::StrCaseEq(link_page_url())))
      .WillOnce(testing::DoAll(
          DelaySendMouseClickToIE(&mock_, &loop_, 0, 1, 1,
                                  simulate_input::LEFT),
          DelaySendScanCode(&loop_, 500, VK_TAB, simulate_input::NONE),
          OpenContextMenu(&loop_, 1000),
          SelectItem(&loop_, 1500, 0)));

  mock_.ExpectNavigationInIE(empty_page_url());
  EXPECT_CALL(mock_, OnIELoad(testing::StrCaseEq(empty_page_url())))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&mock_),
          CloseBrowserMock(&mock_)));

  LaunchIEAndNavigate(link_page_url());
}

TEST_F(NoInterferenceTest, FLAKY_SelectContextMenuOpenInNewWindow) {
  ComStackObjectWithUninitialize<
      testing::StrictMock<MockWebBrowserEventSink> > new_window_mock;
  int open_new_window_index = 2;
  if (chrome_frame_test::GetInstalledIEVersion() == IE_6)
    open_new_window_index = 1;

  // Focus the renderer window by clicking and then tab once to highlight the
  // link.
  mock_.ExpectNavigationInIE(link_page_url());
  EXPECT_CALL(mock_, OnIELoad(testing::StrCaseEq(link_page_url())))
      .WillOnce(testing::DoAll(
          DelaySendMouseClickToIE(&mock_, &loop_, 0, 1, 1,
                                  simulate_input::LEFT),
          DelaySendScanCode(&loop_, 500, VK_TAB, simulate_input::NONE),
          OpenContextMenu(&loop_, 1000),
          SelectItem(&loop_, 1500, open_new_window_index)));

  mock_.ExpectNewWindowWithIE(empty_page_url(), &new_window_mock);
  // The DocumentComplete at times fires twice for new windows. Hack to account
  // for that.
  // TODO(kkania): Verifying the address bar is flaky with this, at least
  // on XP ie6. Fix.
  EXPECT_CALL(new_window_mock,
              OnIELoad(testing::StrCaseEq(empty_page_url())))
      .Times(testing::AtMost(2))
      .WillOnce(DelayCloseBrowserMock(&loop_, 2000, &new_window_mock));

  EXPECT_CALL(new_window_mock, OnQuit()).WillOnce(CloseBrowserMock(&mock_));

  LaunchIEAndNavigate(link_page_url());
}

}  // namespace
