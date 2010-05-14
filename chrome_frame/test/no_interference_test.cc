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
    CloseIeAtEndOfScope last_resort_close_ie;

    EXPECT_CALL(mock_, OnQuit())
        .Times(testing::AtMost(1))
        .WillOnce(QUIT_LOOP(loop_));
    HRESULT hr = mock_.LaunchIEAndNavigate(url);
    ASSERT_HRESULT_SUCCEEDED(hr);
    if (hr == S_FALSE)
      return;

    ASSERT_TRUE(mock_.web_browser2() != NULL);
    loop_.RunFor(kChromeFrameLongNavigationTimeoutInSeconds);
  }

  const std::wstring GetTestUrl(const wchar_t* relative_path) {
    std::wstring path = std::wstring(L"files/no_interference/") +
        relative_path;
    return UTF8ToWide(server_.Resolve(path.c_str()).spec());
  }

  chrome_frame_test::TimedMsgLoop loop_;
  ComStackObjectWithUninitialize<
      testing::StrictMock<MockWebBrowserEventSink> > mock_;
};

ACTION_P(ExpectIERendererWindowHasFocus, mock) {
  mock->ExpectIERendererWindowHasFocus();
}

// This tests that a new IE renderer window has focus.
TEST_F(NoInterferenceTest, SimpleFocus) {
  const std::wstring kEmptyFileUrl = GetTestUrl(L"empty.html");
  mock_.ExpectNavigationAndSwitch(kEmptyFileUrl);

  EXPECT_CALL(mock_, OnIELoad(testing::StrCaseEq(kEmptyFileUrl)))
      .WillOnce(testing::DoAll(
          ExpectIERendererWindowHasFocus(&mock_),
          VerifyAddressBarUrl(&mock_),
          CloseBrowserMock(&mock_)));

  LaunchIEAndNavigate(kEmptyFileUrl);
}

// This tests that window.open does not get intercepted by Chrome Frame.
TEST_F(NoInterferenceTest, FLAKY_JavascriptWindowOpen) {
  const std::wstring kEmptyFileUrl = GetTestUrl(L"empty.html");
  const std::wstring kWindowOpenUrl = GetTestUrl(L"window_open.html");
  ComStackObjectWithUninitialize<
      testing::StrictMock<MockWebBrowserEventSink> > new_window_mock;

  mock_.ExpectNavigationAndSwitch(kWindowOpenUrl);
  EXPECT_CALL(mock_, OnIELoad(testing::StrCaseEq(kWindowOpenUrl)));

  mock_.ExpectNewWindow(&new_window_mock);
  EXPECT_CALL(new_window_mock, OnIELoad(testing::StrCaseEq(kEmptyFileUrl)))
      .WillOnce(CloseBrowserMock(&new_window_mock));
  EXPECT_CALL(new_window_mock, OnQuit())
      .WillOnce(CloseBrowserMock(&mock_));

  LaunchIEAndNavigate(kWindowOpenUrl);
}

}  // namespace