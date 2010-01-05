// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_BHO_H_
#define CHROME_FRAME_BHO_H_

#include <string>

#include <atlbase.h>
#include <atlcom.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <shdeprecated.h>

#include "base/lazy_instance.h"
#include "base/thread_local.h"
#include "chrome_tab.h"  // NOLINT
#include "chrome_frame/resource.h"
#include "grit/chrome_frame_resources.h"

class PatchHelper {
 public:
  enum State { UNKNOWN, PATCH_IBROWSER, PATCH_IBROWSER_OK, PATCH_PROTOCOL };
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
    public IDispEventSimpleImpl<0, Bho, &DIID_DWebBrowserEvents2> {
 public:
  typedef HRESULT (STDMETHODCALLTYPE* IBrowserService_OnHttpEquiv_Fn)(
      IBrowserService* browser, IShellView* shell_view, BOOL done,
      VARIANT* in_arg, VARIANT* out_arg);

DECLARE_REGISTRY_RESOURCEID(IDR_BHO)
DECLARE_NOT_AGGREGATABLE(Bho)
DECLARE_PROTECT_FINAL_CONSTRUCT()

BEGIN_COM_MAP(Bho)
  COM_INTERFACE_ENTRY(IObjectWithSite)
END_COM_MAP()

BEGIN_SINK_MAP(Bho)
 SINK_ENTRY_INFO(0, DIID_DWebBrowserEvents2, DISPID_BEFORENAVIGATE2,
                 BeforeNavigate2, &kBeforeNavigate2Info)
END_SINK_MAP()

  // Lifetime management methods
  Bho();

  HRESULT FinalConstruct();
  void FinalRelease();

  // IObjectWithSite
  STDMETHODIMP SetSite(IUnknown* site);
  STDMETHOD(BeforeNavigate2)(IDispatch* dispatch, VARIANT* url, VARIANT* flags,
      VARIANT* target_frame_name, VARIANT* post_data, VARIANT* headers,
      VARIANT_BOOL* cancel);

  // mshtml sends an IOleCommandTarget::Exec of OLECMDID_HTTPEQUIV
  // (and OLECMDID_HTTPEQUIV_DONE) as soon as it parses a meta tag.
  // It also sends contents of the meta tag as an argument. IEFrame
  // handles this in IBrowserService::OnHttpEquiv.  So this allows
  // us to sniff the META tag by simply patching it. The renderer
  // switching can be achieved by cancelling original navigation
  // and issuing a new one using IWebBrowser2->Navigate2.
  static HRESULT STDMETHODCALLTYPE OnHttpEquiv(
      IBrowserService_OnHttpEquiv_Fn original_httpequiv,
      IBrowserService* browser, IShellView* shell_view, BOOL done,
      VARIANT* in_arg, VARIANT* out_arg);

  std::string referrer() const {
    return referrer_;
  }

  // Returns the Bho instance for the current thread. This is returned from
  // TLS.
  static Bho* GetCurrentThreadBhoInstance();

 protected:
  bool PatchProtocolHandler(const CLSID& handler_clsid);

  std::string referrer_;

  static base::LazyInstance<base::ThreadLocalPointer<Bho> >
      bho_current_thread_instance_;
  static _ATL_FUNC_INFO kBeforeNavigate2Info;
};

#endif  // CHROME_FRAME_BHO_H_

