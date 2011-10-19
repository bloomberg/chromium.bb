// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_BHO_H_
#define CHROME_FRAME_BHO_H_

#include <atlbase.h>
#include <atlcom.h>
#include <deletebrowsinghistory.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <shdeprecated.h>

#include <string>

#include "chrome_frame/chrome_tab.h"
#include "chrome_frame/delete_chrome_history.h"
#include "chrome_frame/resource.h"
#include "chrome_frame/urlmon_moniker.h"
#include "chrome_frame/urlmon_url_request.h"
#include "grit/chrome_frame_resources.h"

class DeleteChromeHistory;

class PatchHelper {
 public:
  enum State { UNKNOWN, PATCH_IBROWSER, PATCH_PROTOCOL, PATCH_MONIKER };
  PatchHelper() : state_(UNKNOWN) {
  }

  State state() const {
    return state_;
  }

  // Returns true if protocols were patched, false if patching has already
  // been done.
  bool InitializeAndPatchProtocolsIfNeeded();

  void PatchBrowserService(IBrowserService* p);
  void UnpatchIfNeeded();
 protected:
  State state_;
};

// Single global variable
extern PatchHelper g_patch_helper;

class ATL_NO_VTABLE Bho
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CComCoClass<Bho, &CLSID_ChromeFrameBHO>,
      public IObjectWithSiteImpl<Bho>,
      public IDispEventSimpleImpl<0, Bho, &DIID_DWebBrowserEvents2>,
      public NavigationManager {
 public:
  typedef HRESULT (STDMETHODCALLTYPE* IBrowserService_OnHttpEquiv_Fn)(
      IBrowserService* browser, IShellView* shell_view, BOOL done,
      VARIANT* in_arg, VARIANT* out_arg);

DECLARE_GET_CONTROLLING_UNKNOWN()
DECLARE_REGISTRY_RESOURCEID(IDR_BHO)
DECLARE_NOT_AGGREGATABLE(Bho)
DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(Bho)
  COM_INTERFACE_ENTRY(IObjectWithSite)
  // When calling DeleteChromeHistory, ensure that only one instance
  // is created to avoid mulitple message loops.
  COM_INTERFACE_ENTRY_CACHED_TEAR_OFF(IID_IDeleteBrowsingHistory,
                                      DeleteChromeHistory,
                                      delete_chrome_history_.p)
END_COM_MAP()

BEGIN_SINK_MAP(Bho)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2,
                  BeforeNavigate2, &kBeforeNavigate2Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_NAVIGATECOMPLETE2,
                  NavigateComplete2, &kNavigateComplete2Info)
  SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_DOCUMENTCOMPLETE,
                  DocumentComplete, &kDocumentCompleteInfo)
END_SINK_MAP()

  Bho();

  HRESULT FinalConstruct();
  void FinalRelease();

  // IObjectWithSite
  STDMETHODIMP SetSite(IUnknown* site);

  // WebBrowser2 event sinks.
  STDMETHOD(BeforeNavigate2)(IDispatch* dispatch, VARIANT* url, VARIANT* flags,
      VARIANT* target_frame_name, VARIANT* post_data, VARIANT* headers,
      VARIANT_BOOL* cancel);
  STDMETHOD_(void, NavigateComplete2)(IDispatch* dispatch, VARIANT* url);
  STDMETHOD_(void, DocumentComplete)(IDispatch* dispatch, VARIANT* url);

  // mshtml sends an IOleCommandTarget::Exec of OLECMDID_HTTPEQUIV
  // (and OLECMDID_HTTPEQUIV_DONE) as soon as it parses a meta tag.
  // It also sends contents of the meta tag as an argument. IEFrame
  // handles this in IBrowserService::OnHttpEquiv.  So this allows
  // us to sniff the META tag by simply patching it. The renderer
  // switching can be achieved by canceling original navigation
  // and issuing a new one using IWebBrowser2->Navigate2.
  static HRESULT STDMETHODCALLTYPE OnHttpEquiv(
      IBrowserService_OnHttpEquiv_Fn original_httpequiv,
      IBrowserService* browser, IShellView* shell_view, BOOL done,
      VARIANT* in_arg, VARIANT* out_arg);

  static void ProcessOptInUrls(IWebBrowser2* browser, BSTR url);

  // COM_INTERFACE_ENTRY_CACHED_TEAR_OFF manages the raw pointer from CComPtr
  // which base::win::ScopedComPtr doesn't expose.
  CComPtr<IUnknown> delete_chrome_history_;

 protected:
  bool PatchProtocolHandler(const CLSID& handler_clsid);

  static _ATL_FUNC_INFO kBeforeNavigate2Info;
  static _ATL_FUNC_INFO kNavigateComplete2Info;
  static _ATL_FUNC_INFO kDocumentCompleteInfo;
};

#endif  // CHROME_FRAME_BHO_H_
