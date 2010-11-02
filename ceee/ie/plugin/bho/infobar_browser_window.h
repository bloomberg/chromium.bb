// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Infobar browser window. This is the window that hosts CF and navigates it
// to the infobar URL.

#ifndef CEEE_IE_PLUGIN_BHO_INFOBAR_BROWSER_WINDOW_H_
#define CEEE_IE_PLUGIN_BHO_INFOBAR_BROWSER_WINDOW_H_

#include <atlbase.h>
#include <atlapp.h>  // Must be included AFTER base.
#include <atlcrack.h>
#include <atlgdi.h>
#include <atlmisc.h>

#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/ie/plugin/bho/infobar_events_funnel.h"

#include "chrome_tab.h"  // NOLINT
#include "toolband.h"  // NOLINT

namespace infobar_api {

class InfobarBrowserWindow;

typedef IDispEventSimpleImpl<0, InfobarBrowserWindow, &DIID_DIChromeFrameEvents>
    ChromeFrameEvents;

// The window that hosts CF where infobar URL is loaded. It implements a limited
// site functionality needed for CF as well as handles sink events from CF.
// TODO(vadimb@google.com): Refactor this class, ChromeFrameHost and ToolBand
// to have a common functionality supported in a common base class.
class ATL_NO_VTABLE InfobarBrowserWindow
    : public CComObjectRootEx<CComSingleThreadModel>,
      public IObjectWithSiteImpl<InfobarBrowserWindow>,
      public IServiceProviderImpl<InfobarBrowserWindow>,
      public IChromeFramePrivileged,
      public ChromeFrameEvents,
      public CWindowImpl<InfobarBrowserWindow> {
 public:
  // Class to connect this an instance of InfobarBrowserWindow with a hosting
  // object who should inherit from InfobarBrowserWindow::Delegate and set it
  // with set_delegate() functions.
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Informs about window.close() event.
    virtual void OnWindowClose() = 0;
  };

  InfobarBrowserWindow();
  ~InfobarBrowserWindow();

  BEGIN_COM_MAP(InfobarBrowserWindow)
    COM_INTERFACE_ENTRY(IServiceProvider)
    COM_INTERFACE_ENTRY(IChromeFramePrivileged)
  END_COM_MAP()

  BEGIN_SERVICE_MAP(InfobarBrowserWindow)
    SERVICE_ENTRY(SID_ChromeFramePrivileged)
    SERVICE_ENTRY_CHAIN(m_spUnkSite)
  END_SERVICE_MAP()

  BEGIN_SINK_MAP(InfobarBrowserWindow)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONREADYSTATECHANGED,
                    OnCfReadyStateChanged, &handler_type_long_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONEXTENSIONREADY,
                    OnCfExtensionReady, &handler_type_bstr_i4_)
    SINK_ENTRY_INFO(0, DIID_DIChromeFrameEvents,
                    CF_EVENT_DISPID_ONCLOSE,
                    OnCfClose, &handler_type_void_)
  END_SINK_MAP()

  BEGIN_MSG_MAP(InfobarBrowserWindow)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_DESTROY(OnDestroy)
  END_MSG_MAP()

  // @name IChromeFramePrivileged implementation.
  // @{
  STDMETHOD(GetWantsPrivileged)(boolean *wants_privileged);
  STDMETHOD(GetChromeExtraArguments)(BSTR *args);
  STDMETHOD(GetChromeProfileName)(BSTR *args);
  STDMETHOD(GetExtensionApisToAutomate)(BSTR *args);
  // @}

  // @name ChromeFrame event handlers.
  // @{
  STDMETHOD_(void, OnCfReadyStateChanged)(LONG state);
  STDMETHOD_(void, OnCfExtensionReady)(BSTR path, int response);
  STDMETHOD_(void, OnCfClose)();
  // @}

  // Initializes the browser window to the given site.
  HRESULT Initialize(HWND parent);
  // Tears down an initialized browser window.
  HRESULT Teardown();

  // Navigates the browser to the given URL if the browser has already been
  // created, otherwise stores the URL to navigate later on.
  void SetUrl(const std::wstring& url);

  // Set the delegate to be informed about window.close() events.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Unit test seam.
  virtual InfobarEventsFunnel& infobar_events_funnel() {
    return infobar_events_funnel_;
  }

 protected:
  // @name Message handlers.
  // @{
  LRESULT OnCreate(LPCREATESTRUCT lpCreateStruct);
  void OnDestroy();
  // @}

 private:
  // The funnel for sending infobar events to the broker.
  InfobarEventsFunnel infobar_events_funnel_;

  // Our Chrome frame instance.
  CComPtr<IChromeFrame> chrome_frame_;

  // Url to navigate infobar to.
  std::wstring url_;
  // Filesystem path to the .crx we will install, or the empty string, or
  // (if not ending in .crx) the path to an exploded extension directory to
  // load.
  std::wstring extension_path_;

  // Delegate. Not owned by the instance of this object.
  Delegate* delegate_;

  static _ATL_FUNC_INFO handler_type_long_;
  static _ATL_FUNC_INFO handler_type_bstr_i4_;
  static _ATL_FUNC_INFO handler_type_void_;

  // Subroutine of general initialization. Extracted to make testable.
  virtual HRESULT InitializeAndShowWindow(HWND parent);

  // Navigate the browser to url_ if the browser has been created.
  void Navigate();

  DISALLOW_COPY_AND_ASSIGN(InfobarBrowserWindow);
};

}  // namespace infobar_api

#endif  // CEEE_IE_PLUGIN_BHO_INFOBAR_BROWSER_WINDOW_H_
