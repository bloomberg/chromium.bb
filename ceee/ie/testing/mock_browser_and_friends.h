// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of TestBrowser and related objects.
#ifndef CEEE_IE_TESTING_MOCK_BROWSER_AND_FRIENDS_H_
#define CEEE_IE_TESTING_MOCK_BROWSER_AND_FRIENDS_H_

#include <string>

#include "gmock/gmock.h"
#include "ceee/common/initializing_coclass.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/test_utils.h"

namespace testing {

class MockIOleWindow : public IOleWindow {
 public:
  // Simple IOleWindow implementation here, no need for a MockXxxImpl.
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetWindow, HRESULT(HWND* window));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, ContextSensitiveHelp, HRESULT(BOOL));
};

class TestBrowserSite
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InstanceCountMixin<TestBrowserSite>,
      public InitializingCoClass<TestBrowserSite>,
      public IServiceProviderImpl<TestBrowserSite>,
      public StrictMock<MockIOleWindow> {
 public:
  BEGIN_COM_MAP(TestBrowserSite)
    COM_INTERFACE_ENTRY(IServiceProvider)
    COM_INTERFACE_ENTRY(IOleWindow)
  END_COM_MAP()

  BEGIN_SERVICE_MAP(TestDumbSite)
    if (browser_ && (guidService == SID_SWebBrowserApp ||
                     guidService == SID_SShellBrowser)) {
      return browser_->QueryInterface(riid, ppvObject);
    }
  END_SERVICE_MAP()

  HRESULT Initialize(TestBrowserSite** self) {
    *self = this;
    return S_OK;
  }

 public:
  CComPtr<IDispatch> browser_;
};

class TestBrowser
    : public CComObjectRootEx<CComSingleThreadModel>,
      public InstanceCountMixin<TestBrowser>,
      public InitializingCoClass<TestBrowser>,
      public StrictMock<IWebBrowser2MockImpl>,
      public IConnectionPointImpl<TestBrowser, &DIID_DWebBrowserEvents2>,
      public IConnectionPointContainerImpl<TestBrowser> {
 public:
  typedef IConnectionPointImpl<TestBrowser, &DIID_DWebBrowserEvents2>
      WebBrowserEvents;

  BEGIN_COM_MAP(TestBrowser)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IWebBrowser)
    COM_INTERFACE_ENTRY(IWebBrowserApp)
    COM_INTERFACE_ENTRY(IWebBrowser2)
    COM_INTERFACE_ENTRY(IConnectionPointContainer)
  END_COM_MAP()

  BEGIN_CONNECTION_POINT_MAP(TestBrowser)
    CONNECTION_POINT_ENTRY(DIID_DWebBrowserEvents2)
  END_CONNECTION_POINT_MAP()

  // Override from IConnectionPointContainerImpl.
  STDMETHOD(FindConnectionPoint)(REFIID iid, IConnectionPoint** cp) {
    typedef IConnectionPointContainerImpl<TestBrowser> CPC;

    if (iid == DIID_DWebBrowserEvents2 && no_events_)
      return CONNECT_E_NOCONNECTION;

    return CPC::FindConnectionPoint(iid, cp);
  }

  void FireOnNavigateComplete(IDispatch* browser, VARIANT *url) {
    CComVariant args[] = { 0, browser };
    args[0].vt = VT_BYREF | VT_BSTR;
    args[0].pvarVal = url;

    testing::FireEvent(static_cast<WebBrowserEvents*>(this),
                       DISPID_NAVIGATECOMPLETE2,
                       arraysize(args),
                       args);
  }

  TestBrowser() : no_events_(false) {
  }

  HRESULT Initialize(TestBrowser** self) {
    *self = this;
    return S_OK;
  }

 public:
  // If true, won't return the WebBrowserEvents connection point.
  bool no_events_;
};

}  // namespace testing

#endif  // CEEE_IE_TESTING_MOCK_BROWSER_AND_FRIENDS_H_
