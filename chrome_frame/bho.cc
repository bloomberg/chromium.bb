// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/bho.h"

#include <shlguid.h>
#include <shobjidl.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
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
#include "net/http/http_util.h"

const wchar_t kPatchProtocols[] = L"PatchProtocols";
static const int kIBrowserServiceOnHttpEquivIndex = 30;

base::LazyInstance<base::ThreadLocalPointer<Bho> >
    Bho::bho_current_thread_instance_(base::LINKER_INITIALIZED);

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
    // Save away our BHO instance in TLS which enables it to be referenced by
    // our active document/activex instances to query referrer and other
    // information for a URL.
    AddRef();
    bho_current_thread_instance_.Pointer()->Set(this);
  } else {
    bho_current_thread_instance_.Pointer()->Set(NULL);
    Release();
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

  referrer_.clear();

  // Save away the referrer in case our active document needs it to initiate
  // navigation in chrome.
  if (headers && V_VT(headers) == VT_BSTR && headers->bstrVal != NULL) {
    std::string raw_headers_utf8 = WideToUTF8(headers->bstrVal);
    std::string http_headers =
        net::HttpUtil::AssembleRawHeaders(raw_headers_utf8.c_str(),
                                          raw_headers_utf8.length());
    net::HttpUtil::HeadersIterator it(http_headers.begin(), http_headers.end(),
                                      "\r\n");
    while (it.GetNext()) {
      if (LowerCaseEqualsASCII(it.name(), "referer")) {
        referrer_ = it.values();
        break;
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

HRESULT Bho::OnHttpEquiv(IBrowserService_OnHttpEquiv_Fn original_httpequiv,
    IBrowserService* browser, IShellView* shell_view, BOOL done,
    VARIANT* in_arg, VARIANT* out_arg) {
  // OnHttpEquiv is invoked for meta tags within sub frames as well.
  // We want to switch renderers only for the top level frame.
  bool is_top_level = (browser->GetBrowserIndex() == -1);

  // OnHttpEquiv with 'done' set to TRUE is called for all pages.
  // 0 or more calls with done set to FALSE are made.
  // When done is FALSE, the current moniker may not represent the page
  // being navigated to so we always have to wait for done to be TRUE
  // before re-initiating the navigation.

  if (done) {
    if (CheckForCFNavigation(browser, false)) {
      ScopedComPtr<IOleObject> mshtml_ole_object;
      HRESULT hr = shell_view->GetItemObject(SVGIO_BACKGROUND, IID_IOleObject,
          reinterpret_cast<void**>(mshtml_ole_object.Receive()));
      DCHECK(FAILED(hr) || mshtml_ole_object != NULL);
      if (mshtml_ole_object) {
        ScopedComPtr<IMoniker> moniker;
        hr = mshtml_ole_object->GetMoniker(OLEGETMONIKER_ONLYIFTHERE,
            OLEWHICHMK_OBJFULL, moniker.Receive());
        DCHECK(FAILED(hr) || moniker != NULL);
        if (moniker) {
          DLOG(INFO) << "Navigating in CF";
          ScopedComPtr<IBindCtx> bind_context;
          // This bind context will be discarded by IE and a new one
          // constructed, so it's OK to create a sync bind context.
          ::CreateBindCtx(0, bind_context.Receive());
          DCHECK(bind_context);
          hr = NavigateBrowserToMoniker(browser, moniker, bind_context);
        } else {
          DLOG(ERROR) << "Couldn't get the current moniker";
        }
      }

      if (FAILED(hr)) {
        NOTREACHED();
        // Lower the flag.
        CheckForCFNavigation(browser, true);
      } else {
        // The navigate-in-gcf flag will be cleared in
        // HttpNegotiatePatch::ReportProgress when the mime type is reported.
      }
    }
  } else if (is_top_level && in_arg && VT_BSTR == V_VT(in_arg)) {
    if (StrStrI(V_BSTR(in_arg), kChromeContentPrefix)) {
      MarkBrowserOnThreadForCFNavigation(browser);
    }
  }

  return original_httpequiv(browser, shell_view, done, in_arg, out_arg);
}

Bho* Bho::GetCurrentThreadBhoInstance() {
  DCHECK(bho_current_thread_instance_.Pointer()->Get() != NULL);
  return bho_current_thread_instance_.Pointer()->Get();
}

namespace {
// Utility function that prevents the current module from ever being unloaded.
void PinModule() {
  FilePath module_path;
  if (PathService::Get(base::FILE_MODULE, &module_path)) {
    HMODULE unused;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_PIN,
                           module_path.value().c_str(), &unused)) {
      NOTREACHED() << "Failed to pin module " << module_path.value().c_str()
                   << " , last error: " << GetLastError();
    }
  } else {
    NOTREACHED() << "Could not get module path.";
  }
}
}  // namespace

bool PatchHelper::InitializeAndPatchProtocolsIfNeeded() {
  bool ret = false;

  _pAtlModule->m_csStaticDataInitAndTypeInfo.Lock();

  if (state_ == UNKNOWN) {
    // If we're going to start patching things, we'd better make sure that we
    // stick around for ever more:
    PinModule();

    HttpNegotiatePatch::Initialize();

    bool patch_protocol = GetConfigBool(true, kPatchProtocols);
    if (patch_protocol) {
      ProtocolSinkWrap::PatchProtocolHandlers();
      state_ = PATCH_PROTOCOL;
    } else {
      state_ = PATCH_IBROWSER;
    }
    ret = true;
  }

  _pAtlModule->m_csStaticDataInitAndTypeInfo.Unlock();

  return ret;
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

