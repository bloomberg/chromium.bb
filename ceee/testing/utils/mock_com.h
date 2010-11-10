// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mock objects for various COM interfaces.

#ifndef CEEE_TESTING_UTILS_MOCK_COM_H_
#define CEEE_TESTING_UTILS_MOCK_COM_H_

#include <atlbase.h>
#include <atlcom.h>
#include <dispex.h>
#include <exdisp.h>
#include <mshtml.h>
#include <objsafe.h>
#include <tlogstg.h>

#include "third_party/activscp/activscp.h"
#include "third_party/activscp/activdbg.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace testing {

class IOleClientSiteMockImpl : public IOleClientSite {
  // The methods in this class are code generated using this command line:
  // [ ceee\testing\utils\com_mock.py IOleClientSite \
  //   "%WindowsSdkDir%\Include\OleIdl.h" ]
#include "ceee/testing/utils/mock_ioleclientsite.gen"
};

class IOleObjecMockImpl: public IOleObject {
 public:
  // The methods in this class are code generated using this command line:
  // [ ceee\testing\utils\com_mock.py IOleObject \
  //   "%WindowsSdkDir%\Include\OleIdl.h" ]
#include "ceee/testing/utils/mock_ioleobject.gen"
};

class IServiceProviderMockImpl : public IServiceProvider {
 public:
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, QueryService,
      HRESULT(REFGUID guidService, REFIID riid, void **ppvObject));
};

class IWebBrowser2MockImpl : public IDispatchImpl<IWebBrowser2,
                                                  &IID_IWebBrowser2,
                                                  &LIBID_SHDocVw,
                                                  1,
                                                  1> {
 public:
  // The methods in this class are code generated using this command line:
  // [ ceee\testing\utils\com_mock.py IWebBrowser2 \
  //   "%WindowsSdkDir%\Include\ExDisp.h" ]
#include "ceee/testing/utils/mock_iwebbrowser2.gen"
};

class IObjectSafetyMockImpl : public IObjectSafety {
 public:
  // The methods in this class are code generated using this command line:
  // [ ceee\testing\utils\com_mock.py IObjectSafety \
  //   "%WindowsSdkDir%\Include\ObjSafe.h" ]
#include "ceee/testing/utils/mock_iobjectsafety.gen"
};

class IActiveScriptMockImpl : public IActiveScript {
 public:
  // The methods in this class are code generated using this command line:
  // [ ceee\testing\utils\com_mock.py IActiveScript \
  //   third_party\activescp\activescp.h ]
#include "ceee/testing/utils/mock_iactivescript.gen"
};

class IActiveScriptParseMockImpl : public IActiveScriptParse {
 public:
  // Unfortunately, MOCK_METHODx only exists for x = [0, 10]
  // and AddScriptlet has 11 arguments. Lucky for us, this interface
  // only has 3 methods.
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, InitNew, HRESULT());
  MOCK_METHOD9_WITH_CALLTYPE(__stdcall, ParseScriptText, HRESULT(
      LPCOLESTR pstrCode, LPCOLESTR pstrItemName, IUnknown *punkContext,
      LPCOLESTR pstrDelimiter, DWORD dwSourceContextCookie,
      ULONG ulStartingLineNumber, DWORD dwFlags, VARIANT *pvarResult,
      EXCEPINFO *pexcepinfo));

  // This will not work with GMock as it will not give any indication
  // as to whether it has been called or not.
  HRESULT __stdcall AddScriptlet(
      LPCOLESTR pstrDefaultName, LPCOLESTR pstrCode, LPCOLESTR pstrItemName,
      LPCOLESTR pstrSubItemName, LPCOLESTR pstrEventName,
      LPCOLESTR pstrDelimiter, DWORD dwSourceContextCookie,
      ULONG ulStartingLineNumber, DWORD dwFlags, BSTR *pbstrName,
      EXCEPINFO *pexcepinfo) {
    return E_NOTIMPL;
  }
};

class IActiveScriptSiteMockImpl : public IActiveScriptSite {
 public:
  // The methods in this class are code generated using this command line:
  // [ ceee\testing\utils\com_mock.py IActiveScriptSite \
  //   third_party\activescp\activescp.h ]
#include "ceee/testing/utils/mock_iactivescriptsite.gen"
};

class IActiveScriptSiteDebugMockImpl : public IActiveScriptSiteDebug {
 public:
  // The methods in this class are code generated using this command line:
  // [ ceee\testing\utils\com_mock.py IActiveScriptSiteDebug \
  //   third_party\activescp\activedbg.h ]
#include "ceee/testing/utils/mock_iactivescriptsitedebug.gen"
};

class IProcessDebugManagerMockImpl : public IProcessDebugManager {
 public:
  // The methods in this class are code generated using this command line:
  // [ ceee\testing\utils\com_mock.py IProcessDebugManager
  //   third_party\activscp\activdbg.h ]
#include "ceee/testing/utils/mock_iprocessdebugmanager.gen"
};

class IDebugApplicationMockImpl : public IDebugApplication {
 public:
  // The methods in this class are code generated using this command line:
  // [ ceee\testing\utils\com_mock.py IDebugApplication \
  //   third_party\activscp\activdbg.h ]
#include "ceee/testing/utils/mock_idebugapplication.gen"
};

class IDebugDocumentHelperMockImpl : public IDebugDocumentHelper {
 public:
  // The methods in this class are code generated using this command line:
  // [ ceee\testing\utils\com_mock.py IDebugDocumentHelper \
  //   third_party\activscp\activdbg.h ]
#include "ceee/testing/utils/mock_idebugdocumenthelper.gen"
};

class ITravelLogStgMockImpl : public ITravelLogStg {
 public:
  // The methods in this class are code generated using this command line:
  // [ ceee\testing\utils\com_mock.py ITravelLogStg \
  //   "%WindowsSdkDir%\Include\tlogstg.h" ]
#include "ceee/testing/utils/mock_itravellogstg.gen"
};

class MockIServiceProvider
    : public CComObjectRootEx<CComSingleThreadModel>,
      public testing::StrictMock<IServiceProviderMockImpl> {
  DECLARE_NOT_AGGREGATABLE(MockIServiceProvider)
  BEGIN_COM_MAP(MockIServiceProvider)
    COM_INTERFACE_ENTRY(IServiceProvider)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT Initialize(MockIServiceProvider** browser) {
    *browser = this;
    return S_OK;
  }
};

class MockIWebBrowser2
  : public CComObjectRootEx<CComSingleThreadModel>,
    public StrictMock<IWebBrowser2MockImpl>,
    public StrictMock<IServiceProviderMockImpl> {
 public:
  DECLARE_NOT_AGGREGATABLE(MockIWebBrowser2)
  BEGIN_COM_MAP(MockIWebBrowser2)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IWebBrowser2)
    COM_INTERFACE_ENTRY(IWebBrowserApp)
    COM_INTERFACE_ENTRY(IWebBrowser)
    COM_INTERFACE_ENTRY(IServiceProvider)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT Initialize(MockIWebBrowser2** browser) {
    *browser = this;
    return S_OK;
  }
};

class MockIProcessDebugManager
  : public CComObjectRootEx<CComSingleThreadModel>,
    public StrictMock<IProcessDebugManagerMockImpl> {
 public:
  BEGIN_COM_MAP(MockIProcessDebugManager)
    COM_INTERFACE_ENTRY(IProcessDebugManager)
  END_COM_MAP()

  HRESULT Initialize(MockIProcessDebugManager** debug_manager) {
    *debug_manager = this;
    return S_OK;
  }
};

class MockIDebugApplication
  : public CComObjectRootEx<CComSingleThreadModel>,
    public StrictMock<IDebugApplicationMockImpl> {
 public:
  BEGIN_COM_MAP(MockIDebugApplication)
    COM_INTERFACE_ENTRY(IDebugApplication)
  END_COM_MAP()

  HRESULT Initialize(MockIDebugApplication** debug_application) {
    *debug_application = this;
    return S_OK;
  }
};

class MockIDebugDocumentHelper
  : public CComObjectRootEx<CComSingleThreadModel>,
    public StrictMock<IDebugDocumentHelperMockImpl> {
 public:
  BEGIN_COM_MAP(MockIDebugDocumentHelper)
    COM_INTERFACE_ENTRY(IDebugDocumentHelper)
  END_COM_MAP();

  HRESULT Initialize(MockIDebugDocumentHelper** debug_document) {
    *debug_document = this;
    return S_OK;
  }
};

class MockITravelLogStg : public CComObjectRootEx<CComSingleThreadModel>,
                          public StrictMock<ITravelLogStgMockImpl> {
 public:
  DECLARE_NOT_AGGREGATABLE(MockITravelLogStg)
  BEGIN_COM_MAP(MockITravelLogStg)
    COM_INTERFACE_ENTRY(ITravelLogStg)
  END_COM_MAP()
  DECLARE_PROTECT_FINAL_CONSTRUCT()
};

}  // namespace testing

#endif  // CEEE_TESTING_UTILS_MOCK_COM_H_
