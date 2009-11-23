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
#include <mshtml.h>
#include <shlguid.h>
#include <shobjidl.h>

#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_variant_win.h"
#include "base/scoped_handle_win.h"
#include "googleurl/src/gurl.h"
#include "chrome_frame/test/http_server.h"
#include "chrome_frame/test_utils.h"
#include "chrome_frame/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

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
// 4. DISPID_NEWWINDOW3
// Other events can be subscribed to on an if needed basis.
class WebBrowserEventSink
    : public CComObjectRootEx<CComSingleThreadModel>,
      public IDispEventSimpleImpl<0, WebBrowserEventSink,
                                  &DIID_DWebBrowserEvents2> {
 public:
  typedef IDispEventSimpleImpl<0, WebBrowserEventSink,
                               &DIID_DWebBrowserEvents2> DispEventsImpl;
  WebBrowserEventSink()
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          onmessage_(this, &WebBrowserEventSink::OnMessageInternal)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          onloaderror_(this, &WebBrowserEventSink::OnLoadErrorInternal)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          onload_(this, &WebBrowserEventSink::OnLoadInternal)) {
  }

  ~WebBrowserEventSink() {
    Uninitialize();
  }

  void Uninitialize();

  // Helper function to launch IE and navigate to a URL.
  // Returns S_OK on success, S_FALSE if the test was not run, other
  // errors on failure.
  HRESULT LaunchIEAndNavigate(const std::wstring& navigate_url);

  HRESULT Navigate(const std::wstring& navigate_url);

  // Set input focus to chrome frame window.
  void SetFocusToChrome();

  // Send keyboard input to the renderer window hosted in chrome using
  // SendInput API
  void SendInputToChrome(const std::string& input_string);

BEGIN_COM_MAP(WebBrowserEventSink)
END_COM_MAP()

BEGIN_SINK_MAP(WebBrowserEventSink)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2,
                 OnBeforeNavigate2Internal, &kBeforeNavigate2Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOWNLOADBEGIN,
                  OnDownloadBegin, &kVoidMethodInfo)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2,
                 OnNavigateComplete2Internal, &kNavigateComplete2Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATEERROR,
                 OnNavigateError, &kNavigateErrorInfo)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW3,
                 OnNewWindow3, &kNewWindow3Info)
END_SINK_MAP()

  STDMETHOD_(void, OnNavigateError)(IDispatch* dispatch, VARIANT* url,
                                    VARIANT* frame_name, VARIANT* status_code,
                                    VARIANT* cancel) {
    DLOG(INFO) << __FUNCTION__;
  }

  STDMETHOD(OnBeforeNavigate2)(IDispatch* dispatch, VARIANT* url, VARIANT*
                               flags, VARIANT* target_frame_name,
                               VARIANT* post_data, VARIANT* headers,
                               VARIANT_BOOL* cancel) {
    return S_OK;
  }

  STDMETHOD(OnBeforeNavigate2Internal)(IDispatch* dispatch, VARIANT* url,
                                       VARIANT* flags,
                                       VARIANT* target_frame_name,
                                       VARIANT* post_data, VARIANT* headers,
                                       VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnDownloadBegin)() {}
  STDMETHOD_(void, OnNavigateComplete2Internal)(IDispatch* dispatch,
                                                VARIANT* url);
  STDMETHOD_(void, OnNavigateComplete2)(IDispatch* dispatch, VARIANT* url) {}
  STDMETHOD_(void, OnNewWindow3)(IDispatch** dispatch, VARIANT_BOOL* Cancel,
                                 DWORD flags, BSTR url_context, BSTR url) {}

#ifdef _DEBUG
  STDMETHOD(Invoke)(DISPID dispid, REFIID riid,
    LCID lcid, WORD flags, DISPPARAMS* params, VARIANT* result,
    EXCEPINFO* except_info, UINT* arg_error) {
    DLOG(INFO) << __FUNCTION__ << L" disp id :"  << dispid;
    return DispEventsImpl::Invoke(dispid, riid, lcid, flags, params, result,
                                  except_info, arg_error);
  }
#endif  // _DEBUG

  // Chrome frame callbacks
  virtual void OnLoad(const wchar_t* url) {}
  virtual void OnLoadError(const wchar_t* url) {}
  virtual void OnMessage(const wchar_t* message) {}

  IWebBrowser2* web_browser2() {
    return web_browser2_.get();
  }

 protected:
  // IChromeFrame callbacks
  HRESULT OnLoadInternal(const VARIANT* param);
  HRESULT OnLoadErrorInternal(const VARIANT* param);
  HRESULT OnMessageInternal(const VARIANT* param);

  void ConnectToChromeFrame();
  HWND GetChromeRendererWindow();

 public:
  ScopedComPtr<IWebBrowser2> web_browser2_;
  ScopedComPtr<IChromeFrame> chrome_frame_;
  DispCallback<WebBrowserEventSink> onmessage_;
  DispCallback<WebBrowserEventSink> onloaderror_;
  DispCallback<WebBrowserEventSink> onload_;

 protected:
  static _ATL_FUNC_INFO kBeforeNavigate2Info;
  static _ATL_FUNC_INFO kNavigateComplete2Info;
  static _ATL_FUNC_INFO kNavigateErrorInfo;
  static _ATL_FUNC_INFO kNewWindow3Info;
  static _ATL_FUNC_INFO kVoidMethodInfo;
};

#endif  // CHROME_FRAME_TEST_CHROME_FRAME_UNITTESTS_H_

