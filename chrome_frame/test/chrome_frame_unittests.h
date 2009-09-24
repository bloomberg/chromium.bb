// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_CHROME_FRAME_UNITTESTS_H_
#define CHROME_FRAME_TEST_CHROME_FRAME_UNITTESTS_H_

#include <atlbase.h>
#include <atlcom.h>
#include <string>
#include <exdisp.h>
#include <exdispid.h>

#include "base/ref_counted.h"
#include "base/scoped_handle_win.h"
#include "googleurl/src/gurl.h"
#include "chrome_frame/test/http_server.h"
#include "testing/gtest/include/gtest/gtest.h"

// Class that:
// 1) Starts the local webserver,
// 2) Supports launching browsers - Internet Explorer and Firefox with local url
// 3) Wait the webserver to finish. It is supposed the test webpage to shutdown
//    the server by navigating to "kill" page
// 4) Supports read the posted results from the test webpage to the "dump"
//    webserver directory
class ChromeFrameTestWithWebServer: public testing::Test {
 protected:
  enum BrowserKind { INVALID, IE, FIREFOX, OPERA, SAFARI, CHROME };

  bool LaunchBrowser(BrowserKind browser, const wchar_t* url);
  bool WaitForTestToComplete(int milliseconds);
  // Waits for the page to notify us of the window.onload event firing.
  // Note that the milliseconds value is only approximate.
  bool WaitForOnLoad(int milliseconds);
  bool ReadResultFile(const std::wstring& file_name, std::string* data);

  // Launches the specified browser and waits for the test to complete
  // (see WaitForTestToComplete).  Then checks that the outcome is OK.
  // This function uses EXPECT_TRUE and ASSERT_TRUE for all steps performed
  // hence no return value.
  void SimpleBrowserTest(BrowserKind browser, const wchar_t* page,
                         const wchar_t* result_file_to_check);

  // Same as SimpleBrowserTest but if the browser isn't installed (LaunchBrowser
  // fails), the function will print out a warning but not treat the test
  // as failed.
  // Currently this is how we run Opera tests.
  void OptionalBrowserTest(BrowserKind browser, const wchar_t* page,
                           const wchar_t* result_file_to_check);

  // Test if chrome frame correctly reports its version.
  void VersionTest(BrowserKind browser, const wchar_t* page,
                   const wchar_t* result_file_to_check);

  void CloseBrowser();

  // Ensures (well, at least tries to ensure) that the browser window has focus.
  bool BringBrowserToTop();

  // Returns true iff the specified result file contains 'expected result'.
  bool CheckResultFile(const std::wstring& file_name,
                       const std::string& expected_result);

  virtual void SetUp();
  virtual void TearDown();

  // Important: kind means "sheep" in Icelandic. ?:-o
  const char* ToString(BrowserKind kind) {
    switch (kind) {
      case IE:
        return "IE";
      case FIREFOX:
        return "Firefox";
      case OPERA:
        return "Opera";
      case CHROME:
        return "Chrome";
      case SAFARI:
        return "Safari";
      default:
        NOTREACHED();
        break;
    }
    return "";
  }

  BrowserKind browser_;
  std::wstring results_dir_;
  ScopedHandle browser_handle_;
  ChromeFrameHTTPServer server_;
};

// This class sets up event sinks to the IWebBrowser interface. Currently it
// subscribes to the following events:-
// 1. DISPID_BEFORENAVIGATE2
// 2. DISPID_NAVIGATEERROR
// 3. DISPID_NAVIGATECOMPLETE2 
// Other events can be subscribed to on an if needed basis.
class WebBrowserEventSink
    : public CComObjectRootEx<CComSingleThreadModel>,
      public IDispEventSimpleImpl<0, WebBrowserEventSink,
                                  &DIID_DWebBrowserEvents2> {
 public:
  WebBrowserEventSink()
      : navigation_failed_(false),
        main_thread_id_(0) {
  }

BEGIN_COM_MAP(WebBrowserEventSink)
END_COM_MAP()

BEGIN_SINK_MAP(WebBrowserEventSink)
 SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2,
                 OnBeforeNavigate2, &kBeforeNavigate2Info)
 SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2,
                 OnNavigateComplete2, &kNavigateComplete2Info)
 SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATEERROR,
                 OnNavigateError, &kNavigateErrorInfo)
END_SINK_MAP()

  STDMETHOD_(void, OnNavigateError)(IDispatch* dispatch, VARIANT* url,
                                    VARIANT* frame_name, VARIANT* status_code,
                                    VARIANT* cancel) {
    navigation_failed_ = true;
  }

  STDMETHOD(OnBeforeNavigate2)(IDispatch* dispatch, VARIANT* url, VARIANT*
                               flags, VARIANT* target_frame_name,
                               VARIANT* post_data, VARIANT* headers,
                               VARIANT_BOOL* cancel) {
    DLOG(INFO) << __FUNCTION__;
    // If a navigation fails then IE issues a navigation to an interstitial
    // page. Catch this to track navigation errors as the NavigateError
    // notification does not seem to fire reliably.
    GURL crack_url(url->bstrVal);
    if (crack_url.scheme() == "res") {
      navigation_failed_ = true;
    }
    return S_OK;
  }

  STDMETHOD_(void, OnNavigateComplete2)(IDispatch* dispatch, VARIANT* url) {
    DLOG(INFO) << __FUNCTION__;
  }

  bool navigation_failed() const {
    return navigation_failed_;
  }

  void set_main_thread_id(DWORD thread_id) {
    main_thread_id_ = thread_id;
  }

 protected:
  bool navigation_failed_;

  static _ATL_FUNC_INFO kBeforeNavigate2Info;
  static _ATL_FUNC_INFO kNavigateComplete2Info;
  static _ATL_FUNC_INFO kNavigateErrorInfo;
  DWORD main_thread_id_;
};

#endif  // CHROME_FRAME_TEST_CHROME_FRAME_UNITTESTS_H_

