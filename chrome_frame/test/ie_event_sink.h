// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_IE_EVENT_SINK_H_
#define CHROME_FRAME_TEST_IE_EVENT_SINK_H_

#include <atlbase.h>
#include <atlwin.h>
#include <exdispid.h>
#include <string>

#include "base/lock.h"
#include "base/scoped_comptr_win.h"

#include "chrome_frame/test_utils.h"
#include "chrome_frame/test/simulate_input.h"

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

namespace chrome_frame_test {

// Listener for all events from the IEEventSink, defined below. This includes
// IE and CF events. Unfortunately some of these events are unreliable or have
// strange behavior across different platforms/browsers. See notes besides
// each method.
class IEEventListener {
 public:
  virtual ~IEEventListener() {}

  // IE callbacks
  virtual void OnNavigateError(IDispatch* dispatch, VARIANT* url,
                               VARIANT* frame_name, VARIANT* status_code,
                               VARIANT* cancel) {}
  // This does not occur in IE 6 in CF when navigating between fragments
  // on the same page, although it does occur with back/forward across such.
  virtual void OnBeforeNavigate2(IDispatch* dispatch, VARIANT* url,
                                 VARIANT* flags, VARIANT* target_frame_name,
                                 VARIANT* post_data, VARIANT* headers,
                                 VARIANT_BOOL* cancel) {}
  virtual void OnDownloadBegin() {}
  virtual void OnNavigateComplete2(IDispatch* dispatch, VARIANT* url) {}
  virtual void OnNewWindow2(IDispatch** dispatch, VARIANT_BOOL* cancel) {}
  virtual void OnNewWindow3(IDispatch** dispatch, VARIANT_BOOL* cancel,
                            DWORD flags, BSTR url_context, BSTR url) {}
  // This occurs twice on IE >= 7 after window.open calls.
  virtual void OnDocumentComplete(IDispatch* dispatch, VARIANT* url_variant) {}
  virtual void OnFileDownload(VARIANT_BOOL active_doc, VARIANT_BOOL* cancel) {}
  virtual void OnQuit() {}

  // CF callbacks
  virtual void OnLoad(const wchar_t* url) {}
  virtual void OnLoadError(const wchar_t* url) {}
  virtual void OnMessage(const wchar_t* message, const wchar_t* origin,
                         const wchar_t* source) {}
  virtual void OnNewBrowserWindow(IDispatch* new_window, const wchar_t* url) {}
};

// Listener for IPropertyNotifySink.
class PropertyNotifySinkListener {
 public:
  virtual ~PropertyNotifySinkListener() {}
  virtual void OnChanged(DISPID dispid) {}
  virtual void OnRequestEdit(DISPID dispid) {}
};

// This class sets up event sinks to the IWebBrowser interface. It forwards
// all events to its listener.
// TODO(kkania): Delete WebBrowserEventSink and use this class instead for
// the reliability tests.
class IEEventSink
    : public CComObjectRootEx<CComSingleThreadModel>,
      public IDispEventSimpleImpl<0, IEEventSink,
                                  &DIID_DWebBrowserEvents2>,
      public IUnknown {
 public:
  // Needed to support PostTask.
  static bool ImplementsThreadSafeReferenceCounting() {
    return true;
  }

  typedef IDispEventSimpleImpl<0, IEEventSink,
                               &DIID_DWebBrowserEvents2> DispEventsImpl;
  IEEventSink();
  ~IEEventSink();

  // Listen to events from this |browser_disp|, which should be queryable for
  // IWebBrowser2.
  void Attach(IDispatch* browser_disp);

  // Stop listening to the associated web browser and possibly wait for it to
  // close, if this browser has its own process.
  void Uninitialize();

  // Closes the web browser in such a way that the OnQuit notification will
  // be fired when the window closes (async).
  HRESULT CloseWebBrowser();

  // Posts a message to the given target in ChromeFrame. |target| may be "*".
  void PostMessageToCF(const std::wstring& message, const std::wstring& target);

    // Set input focus to chrome frame window.
  void SetFocusToRenderer();

  // Send keyboard input to the renderer window hosted in chrome using
  // SendInput API.
  void SendKeys(const wchar_t* input_string);

  // Send mouse click to the renderer window hosted in chrome using
  // SendInput API.
  void SendMouseClick(int x, int y, simulate_input::MouseButton button);

  // Get the HWND for the browser's main window. Will fail test if window
  // not found.
  HWND GetBrowserWindow();

  // Get the HWND for the browser's renderer window. Will fail test if
  // renderer window not found.
  HWND GetRendererWindow();

  // Same as above, but does not fail the test if the window cannot be found.
  // In that case, the returned handle will be NULL.
  HWND GetRendererWindowSafe();

  // Launch IE, use the given listener, and navigate to the given url.
  HRESULT LaunchIEAndNavigate(const std::wstring& navigate_url,
                              IEEventListener* listener);

  // Navigate to the given url.
  HRESULT Navigate(const std::wstring& navigate_url);

  // Returns whether CF is rendering the current page.
  bool IsCFRendering();

  // Expect the renderer window to have focus.
  void ExpectRendererWindowHasFocus();

  // Expect the address bar to have |url|.
  void ExpectAddressBarUrl(const std::wstring& url);

  // These methods are just simple wrappers of the IWebBrowser2 methods.
  // They are needed because you cannot post tasks to IWebBrowser2.
  void GoBack() {
    web_browser2_->GoBack();
  }

  void GoForward() {
    web_browser2_->GoForward();
  }

  void Refresh();

  void Exec(const GUID* cmd_group_guid, DWORD command_id,
            DWORD cmd_exec_opt, VARIANT* in_args, VARIANT* out_args);

  // Set the listener for this sink, which can be NULL.
  void set_listener(IEEventListener* listener) { listener_ = listener; }

  IWebBrowser2* web_browser2() { return web_browser2_.get(); }

  // Used only for debugging/logging purposes.
  bool reference_count() { return m_dwRef; }

 private:
  void ConnectToChromeFrame();
  void DisconnectFromChromeFrame();
  void FindIEProcessId();

  // IE callbacks.
BEGIN_COM_MAP(IEEventSink)
  COM_INTERFACE_ENTRY(IUnknown)
END_COM_MAP()

BEGIN_SINK_MAP(IEEventSink)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2,
                  OnBeforeNavigate2, &kBeforeNavigate2Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOWNLOADBEGIN,
                  OnDownloadBegin, &kVoidMethodInfo)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2,
                  OnNavigateComplete2, &kNavigateComplete2Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATEERROR,
                  OnNavigateError, &kNavigateErrorInfo)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW2,
                  OnNewWindow2, &kNewWindow2Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NEWWINDOW3,
                  OnNewWindow3, &kNewWindow3Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE,
                  OnDocumentComplete, &kDocumentCompleteInfo)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_FILEDOWNLOAD,
                  OnFileDownload, &kFileDownloadInfo)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_ONQUIT,
                  OnQuit, &kVoidMethodInfo)
END_SINK_MAP()

  STDMETHOD_(void, OnNavigateError)(IDispatch* dispatch, VARIANT* url,
                                    VARIANT* frame_name, VARIANT* status_code,
                                    VARIANT* cancel);
  STDMETHOD(OnBeforeNavigate2)(IDispatch* dispatch, VARIANT* url,
                               VARIANT* flags, VARIANT* target_frame_name,
                               VARIANT* post_data, VARIANT* headers,
                               VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnDownloadBegin)();
  STDMETHOD_(void, OnNavigateComplete2)(IDispatch* dispatch, VARIANT* url);
  STDMETHOD_(void, OnNewWindow2)(IDispatch** dispatch, VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnNewWindow3)(IDispatch** dispatch, VARIANT_BOOL* cancel,
                                 DWORD flags, BSTR url_context, BSTR url);
  STDMETHOD_(void, OnDocumentComplete)(IDispatch* dispatch,
                                       VARIANT* url_variant);
  STDMETHOD_(void, OnFileDownload)(VARIANT_BOOL active_doc,
                                   VARIANT_BOOL* cancel);
  STDMETHOD_(void, OnQuit)();

#ifdef _DEBUG
  STDMETHOD(Invoke)(DISPID dispid, REFIID riid,
    LCID lcid, WORD flags, DISPPARAMS* params, VARIANT* result,
    EXCEPINFO* except_info, UINT* arg_error) {
    DLOG(INFO) << __FUNCTION__ << L" disp id :"  << dispid;
    return DispEventsImpl::Invoke(dispid, riid, lcid, flags, params, result,
                                  except_info, arg_error);
  }
#endif  // _DEBUG

  // IChromeFrame callbacks
  HRESULT OnLoad(const VARIANT* param);
  HRESULT OnLoadError(const VARIANT* param);
  HRESULT OnMessage(const VARIANT* param);

  ScopedComPtr<IWebBrowser2> web_browser2_;
  ScopedComPtr<IChromeFrame> chrome_frame_;
  DispCallback<IEEventSink> onmessage_;
  DispCallback<IEEventSink> onloaderror_;
  DispCallback<IEEventSink> onload_;
  IEEventListener* listener_;
  base::ProcessId ie_process_id_;
  bool did_receive_on_quit_;

  static _ATL_FUNC_INFO kBeforeNavigate2Info;
  static _ATL_FUNC_INFO kNavigateComplete2Info;
  static _ATL_FUNC_INFO kNavigateErrorInfo;
  static _ATL_FUNC_INFO kNewWindow2Info;
  static _ATL_FUNC_INFO kNewWindow3Info;
  static _ATL_FUNC_INFO kVoidMethodInfo;
  static _ATL_FUNC_INFO kDocumentCompleteInfo;
  static _ATL_FUNC_INFO kFileDownloadInfo;
};

class PropertyNotifySinkImpl
    : public CComObjectRootEx<CComSingleThreadModel>,
      public IPropertyNotifySink {
 public:
  PropertyNotifySinkImpl() : listener_(NULL) {
  }

BEGIN_COM_MAP(PropertyNotifySinkImpl)
  COM_INTERFACE_ENTRY(IPropertyNotifySink)
END_COM_MAP()

  STDMETHOD(OnChanged)(DISPID dispid) {
    if (listener_)
      listener_->OnChanged(dispid);
    return S_OK;
  }

  STDMETHOD(OnRequestEdit)(DISPID dispid) {
    if (listener_)
      listener_->OnRequestEdit(dispid);
    return S_OK;
  }

  void set_listener(PropertyNotifySinkListener* listener) {
    DCHECK(listener_ == NULL || listener == NULL);
    listener_ = listener;
  }

 protected:
  PropertyNotifySinkListener* listener_;
};

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_IE_EVENT_SINK_H_

