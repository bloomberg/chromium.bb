// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlwin.h>
#include <atlhost.h>

#include "base/win/scoped_comptr.h"
#include "chrome_frame/test/perf/chrome_frame_perftest.h"

interface IXcpControlDownloadCallback;
interface __declspec(uuid("1B36028E-B491-4bb2-8584-8A9E0A677D6E"))
IXcpControlHost : public IUnknown {
  typedef enum {
    XcpHostOption_FreezeOnInitialFrame       = 0x001,
    XcpHostOption_DisableFullScreen          = 0x002,
    XcpHostOption_DisableManagedExecution    = 0x008,
    XcpHostOption_EnableCrossDomainDownloads = 0x010,
    XcpHostOption_UseCustomAppDomain         = 0x020,
    XcpHostOption_DisableNetworking          = 0x040,
    XcpHostOption_DisableScriptCallouts      = 0x080,
    XcpHostOption_EnableHtmlDomAccess        = 0x100,
    XcpHostOption_EnableScriptableObjectAccess = 0x200,
  } XcpHostOptions;

  STDMETHOD(GetHostOptions)(DWORD* pdwOptions) PURE;
  STDMETHOD(NotifyLoaded()) PURE;
  STDMETHOD(NotifyError)(BSTR bstrError, BSTR bstrSource,
      long nLine, long nColumn) PURE;
  STDMETHOD(InvokeHandler)(BSTR bstrName, VARIANT varArg1, VARIANT varArg2,
      VARIANT* pvarResult) PURE;
  STDMETHOD(GetBaseUrl)(BSTR* pbstrUrl) PURE;
  STDMETHOD(GetNamedSource)(BSTR bstrSourceName, BSTR* pbstrSource) PURE;
  STDMETHOD(DownloadUrl)(BSTR bstrUrl, IXcpControlDownloadCallback* pCallback,
      IStream** ppStream) PURE;
};

// Not templatized, to trade execution speed vs typing
class IXcpControlHostImpl : public IXcpControlHost {
 public:
  STDMETHOD(GetHostOptions)(DWORD* pdwOptions) {
    return E_NOTIMPL;
  }

  STDMETHOD(NotifyLoaded()) {
    return E_NOTIMPL;
  }

  STDMETHOD(NotifyError)(BSTR bstrError, BSTR bstrSource,
                         long nLine, long nColumn) {
    return E_NOTIMPL;
  }

  STDMETHOD(InvokeHandler)(BSTR bstrName, VARIANT varArg1, VARIANT varArg2,
                           VARIANT* pvarResult) {
    return E_NOTIMPL;
  }

  STDMETHOD(GetBaseUrl)(BSTR* pbstrUrl) {
    return E_NOTIMPL;
  }

  STDMETHOD(GetNamedSource)(BSTR bstrSourceName, BSTR* pbstrSource) {
    return E_NOTIMPL;
  }

  STDMETHOD(DownloadUrl)(BSTR bstrUrl, IXcpControlDownloadCallback* pCallback,
                         IStream** ppStream) {
    return E_NOTIMPL;
  }
};

// Silverlight container. Supports do-nothing implementation of IXcpControlHost.
// Should be extended to do some real movie-or-something download.
class SilverlightContainer :
    public IServiceProviderImpl<SilverlightContainer>,
    public IXcpControlHostImpl,
    public CWindowImpl<SilverlightContainer, CWindow, CWinTraits<
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        WS_EX_APPWINDOW | WS_EX_WINDOWEDGE> >,
    public CComObjectRootEx<CComSingleThreadModel> {
 public:
  DECLARE_WND_CLASS_EX(L"Silverlight_container", 0, 0)
  BEGIN_COM_MAP(SilverlightContainer)
    COM_INTERFACE_ENTRY(IServiceProvider)
    COM_INTERFACE_ENTRY(IXcpControlHost)
  END_COM_MAP()

  BEGIN_SERVICE_MAP(SilverlightContainer)
    SERVICE_ENTRY(__uuidof(IXcpControlHost))
  END_SERVICE_MAP()

  BEGIN_MSG_MAP(ChromeFrameActiveXContainer)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  END_MSG_MAP()

  LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
    host_.Release();
    return 0;
  }

  virtual void OnFinalMessage(HWND ) {
  }

  static const wchar_t* GetWndCaption() {
    return L"Silverlight Container";
  }

  HRESULT CreateWndAndHost(RECT* r) {
    Create(NULL, r);
    ShowWindow(SW_SHOWDEFAULT);

    CComPtr<IUnknown> spUnkContainer;
    HRESULT hr = CAxHostWindow::_CreatorClass::CreateInstance(NULL,
        __uuidof(IAxWinHostWindow), reinterpret_cast<void**>(&host_));
    if (SUCCEEDED(hr)) {
      CComPtr<IObjectWithSite> p;
      hr = host_.QueryInterface(&p);
      if (SUCCEEDED(hr)) {
        p->SetSite(GetUnknown());
      }
    }
    return hr;
  }

  HRESULT CreateControl() {
    HRESULT hr = host_->CreateControl(L"AgControl.AgControl", m_hWnd, NULL);
    EXPECT_HRESULT_SUCCEEDED(hr);
    return hr;
  }

  base::win::ScopedComPtr<IAxWinHostWindow> host_;
};

// Create and in-place Silverlight control. Should be extended to do something
// more meaningful.
TEST(ChromeFramePerf, DISABLED_HostSilverlight2) {
  SimpleModule module;
  AtlAxWinInit();
  CComObjectStackEx<SilverlightContainer> wnd;
  RECT rc = {0, 0, 800, 600};
  wnd.CreateWndAndHost(&rc);
  PerfTimeLogger perf_create("Create Silverlight Control2");
  wnd.CreateControl();
  perf_create.Done();
  wnd.DestroyWindow();
}

// Simplest test - creates in-place Silverlight control.
TEST(ChromeFramePerf, DISABLED_HostSilverlight) {
  SimpleModule module;
  AtlAxWinInit();
  CAxWindow host;
  RECT rc = {0, 0, 800, 600};
  PerfTimeLogger perf_create("Create Silverlight Control");
  host.Create(NULL, rc, L"AgControl.AgControl",
      WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
      WS_EX_APPWINDOW | WS_EX_WINDOWEDGE);
  EXPECT_TRUE(host.m_hWnd != NULL);
  base::win::ScopedComPtr<IDispatch> disp;
  HRESULT hr = host.QueryControl(disp.Receive());
  EXPECT_HRESULT_SUCCEEDED(hr);
  disp.Release();
  perf_create.Done();
}

