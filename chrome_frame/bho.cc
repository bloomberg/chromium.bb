// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/bho.h"

#include <shlguid.h>
#include <shobjidl.h>

#include "base/logging.h"
#include "base/registry.h"
#include "base/scoped_bstr_win.h"
#include "base/scoped_comptr_win.h"
#include "base/scoped_variant_win.h"
#include "base/string_util.h"
#include "chrome_tab.h" // NOLINT
#include "chrome_frame/http_negotiate.h"
#include "chrome_frame/protocol_sink_wrap.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/vtable_patch_manager.h"

const wchar_t kPatchProtocols[] = L"PatchProtocols";
static const int kIBrowserServiceOnHttpEquivIndex = 30;

PatchHelper g_patch_helper;

BEGIN_VTABLE_PATCHES(IBrowserService)
  VTABLE_PATCH_ENTRY(kIBrowserServiceOnHttpEquivIndex, Bho::OnHttpEquiv)
END_VTABLE_PATCHES()

_ATL_FUNC_INFO Bho::kBeforeNavigate2Info = {
  CC_STDCALL, VT_EMPTY, 7, {
    VT_DISPATCH,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_VARIANT | VT_BYREF,
    VT_BOOL | VT_BYREF
  }
};

Bho::Bho() {
}

STDMETHODIMP Bho::SetSite(IUnknown* site) {
  HRESULT hr = S_OK;
  if (site) {
    ScopedComPtr<IWebBrowser2> web_browser2;
    web_browser2.QueryFrom(site);
    if (web_browser2) {
      hr = DispEventAdvise(web_browser2, &DIID_DWebBrowserEvents2);
      DCHECK(SUCCEEDED(hr)) << "DispEventAdvise failed. Error: " << hr;
    }

    if (g_patch_helper.state() == PatchHelper::PATCH_IBROWSER) {
      ScopedComPtr<IBrowserService> browser_service;
      hr = DoQueryService(SID_SShellBrowser, site, browser_service.Receive());
      DCHECK(browser_service) << "DoQueryService - SID_SShellBrowser failed."
        << " Site: " << site << " Error: " << hr;
      if (browser_service) {
        g_patch_helper.PatchBrowserService(browser_service);
        DCHECK(SUCCEEDED(hr)) << "vtable_patch::PatchInterfaceMethods failed."
            << " Site: " << site << " Error: " << hr;
      }
    }
  }

  return IObjectWithSiteImpl<Bho>::SetSite(site);
}

STDMETHODIMP Bho::BeforeNavigate2(IDispatch* dispatch, VARIANT* url,
    VARIANT* flags, VARIANT* target_frame_name, VARIANT* post_data,
    VARIANT* headers, VARIANT_BOOL* cancel) {
  ScopedComPtr<IWebBrowser2> web_browser2;
  if (dispatch)
    web_browser2.QueryFrom(dispatch);

  if (!web_browser2) {
    NOTREACHED() << "Can't find WebBrowser2 with given dispatch";
    return S_OK;  // Return success, we operate on best effort basis.
  }

  DLOG(INFO) << "BeforeNavigate2: " << url->bstrVal;

  if (g_patch_helper.state() == PatchHelper::PATCH_IBROWSER) {
    VARIANT_BOOL is_top_level = VARIANT_FALSE;
    web_browser2->get_TopLevelContainer(&is_top_level);

    std::wstring current_url;
    bool is_chrome_protocol = false;
    if (is_top_level && IsValidUrlScheme(url->bstrVal, false)) {
      current_url.assign(url->bstrVal, SysStringLen(url->bstrVal));
      is_chrome_protocol = StartsWith(current_url, kChromeProtocolPrefix,
                                      false);

      if (!is_chrome_protocol && IsOptInUrl(current_url.c_str())) {
        DLOG(INFO) << "Canceling navigation and switching to cf";
        // Cancel original navigation
        *cancel = VARIANT_TRUE;

        // Issue new request with 'cf:'
        current_url.insert(0, kChromeProtocolPrefix);
        ScopedVariant new_url(current_url.c_str());
        HRESULT hr = web_browser2->Navigate2(new_url.AsInput(), flags,
                                             target_frame_name, post_data,
                                             headers);
        DCHECK(SUCCEEDED(hr)) << "web_browser2->Navigate2 failed. Error: " << hr
            << std::endl << "Url: " << current_url
            << std::endl << "flags: " << flags
            << std::endl << "post data: " << post_data
            << std::endl << "headers: " << headers;
      }
    }
  }
  return S_OK;
}

HRESULT Bho::FinalConstruct() {
  return S_OK;
}

void Bho::FinalRelease() {
}

HRESULT STDMETHODCALLTYPE Bho::OnHttpEquiv(
    IBrowserService_OnHttpEquiv_Fn original_httpequiv,
    IBrowserService* browser, IShellView* shell_view, BOOL done,
    VARIANT* in_arg, VARIANT* out_arg) {
  if (!done && in_arg && (VT_BSTR == V_VT(in_arg))) {
    if (StrStrI(V_BSTR(in_arg), kChromeContentPrefix)) {
      // OnHttpEquiv is invoked for meta tags within sub frames as well.
      // We want to switch renderers only for the top level frame. Since
      // the same |browser| and |shell_view| are passed in to OnHttpEquiv
      // even for sub iframes, we determine if this is the top one by
      // checking if there are any sub frames created or not.
      ScopedComPtr<IWebBrowser2> web_browser2;
      DoQueryService(SID_SWebBrowserApp, browser, web_browser2.Receive());
      if (web_browser2 && !HasSubFrames(web_browser2))
        SwitchRenderer(web_browser2, browser, shell_view, V_BSTR(in_arg));
    }
  }

  return original_httpequiv(browser, shell_view, done, in_arg, out_arg);
}

bool Bho::HasSubFrames(IWebBrowser2* web_browser2) {
  bool has_sub_frames = false;
  ScopedComPtr<IDispatch> doc_dispatch;
  if (web_browser2) {
    HRESULT hr = web_browser2->get_Document(doc_dispatch.Receive());
    DCHECK(SUCCEEDED(hr) && doc_dispatch) <<
        "web_browser2->get_Document failed. Error: " << hr;
    ScopedComPtr<IOleContainer> container;
    if (SUCCEEDED(hr) && doc_dispatch) {
      container.QueryFrom(doc_dispatch);
      ScopedComPtr<IEnumUnknown> enumerator;
      if (container) {
        container->EnumObjects(OLECONTF_EMBEDDINGS, enumerator.Receive());
        ScopedComPtr<IUnknown> unk;
        ULONG items_retrieved = 0;
        if (enumerator)
          enumerator->Next(1, unk.Receive(), &items_retrieved);
        has_sub_frames = (items_retrieved != 0);
      }
    }
  }

  return has_sub_frames;
}

HRESULT Bho::SwitchRenderer(IWebBrowser2* web_browser2,
    IBrowserService* browser, IShellView* shell_view,
    const wchar_t* meta_tag) {
  DCHECK(web_browser2 && browser && shell_view && meta_tag);

  // Get access to the mshtml instance and the moniker
  ScopedComPtr<IOleObject> mshtml_ole_object;
  HRESULT hr = shell_view->GetItemObject(SVGIO_BACKGROUND, IID_IOleObject,
      reinterpret_cast<void**>(mshtml_ole_object.Receive()));
  if (!mshtml_ole_object) {
    NOTREACHED() << "shell_view->GetItemObject failed. Error: " << hr;
    return hr;
  }

  std::wstring url;
  ScopedComPtr<IMoniker> moniker;
  hr = mshtml_ole_object->GetMoniker(OLEGETMONIKER_ONLYIFTHERE,
                                     OLEWHICHMK_OBJFULL, moniker.Receive());
  DCHECK(moniker) << "mshtml_ole_object->GetMoniker failed. Error: " << hr;

  if (moniker)
    hr = GetUrlFromMoniker(moniker, NULL, &url);

  DCHECK(!url.empty()) << "GetUrlFromMoniker failed. Error: " << hr;
  DCHECK(!StartsWith(url, kChromeProtocolPrefix, false));

  if (!url.empty()) {
    url.insert(0, kChromeProtocolPrefix);
    // Navigate to new url
    VARIANT empty = ScopedVariant::kEmptyVariant;
    VARIANT flags = { VT_I4 };
    V_I4(&flags) = 0;
    ScopedVariant url_var(url.c_str());
    hr = web_browser2->Navigate2(url_var.AsInput(), &flags, &empty, &empty,
                                 &empty);
    DCHECK(SUCCEEDED(hr)) << "web_browser2->Navigate2 failed. Error: " << hr
        << std::endl << "Url: " << url;
  }

  return S_OK;
}

void PatchHelper::InitializeAndPatchProtocolsIfNeeded() {
  if (state_ != UNKNOWN)
    return;

  HttpNegotiatePatch::Initialize();

  bool patch_protocol = GetConfigBool(true, kPatchProtocols);
  if (patch_protocol) {
    ProtocolSinkWrap::PatchProtocolHandlers();
    state_ = PATCH_PROTOCOL;
  } else {
    state_ = PATCH_IBROWSER;
  }
}

void PatchHelper::PatchBrowserService(IBrowserService* browser_service) {
  DCHECK(state_ == PATCH_IBROWSER);
  state_ = PATCH_IBROWSER_OK;
  vtable_patch::PatchInterfaceMethods(browser_service,
                                      IBrowserService_PatchInfo);
}

void PatchHelper::UnpatchIfNeeded() {
  if (state_ == PATCH_PROTOCOL) {
    ProtocolSinkWrap::UnpatchProtocolHandlers();
  } else if (state_ == PATCH_IBROWSER_OK) {
    vtable_patch::UnpatchInterfaceMethods(IBrowserService_PatchInfo);
  }

  HttpNegotiatePatch::Uninitialize();

  state_ = UNKNOWN;
}

