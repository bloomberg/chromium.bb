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
#include "chrome_frame/extra_system_apis.h"
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

HRESULT Bho::FinalConstruct() {
  return S_OK;
}

void Bho::FinalRelease() {
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
  if (!url || url->vt != VT_BSTR || url->bstrVal == NULL) {
    DLOG(WARNING) << "Invalid URL passed in";
    return S_OK;
  }

  ScopedComPtr<IWebBrowser2> web_browser2;
  if (dispatch)
    web_browser2.QueryFrom(dispatch);

  if (!web_browser2) {
    NOTREACHED() << "Can't find WebBrowser2 with given dispatch";
    return S_OK;
  }

  DLOG(INFO) << "BeforeNavigate2: " << url->bstrVal;
  ScopedComPtr<IBrowserService> browser_service;
  DoQueryService(SID_SShellBrowser, web_browser2, browser_service.Receive());
  if (!browser_service || !CheckForCFNavigation(browser_service, false)) {
    referrer_.clear();
  }
  url_ = url->bstrVal;
  ProcessOptInUrls(web_browser2, url->bstrVal);
  return S_OK;
}

HRESULT Bho::NavigateToCurrentUrlInCF(IBrowserService* browser) {
  DCHECK(browser);
  MarkBrowserOnThreadForCFNavigation(browser);
  ScopedComPtr<IBindCtx> bind_context;
  ScopedComPtr<IMoniker> moniker;
  HRESULT hr = ::CreateBindCtx(0, bind_context.Receive());
  DCHECK(bind_context);
  if (SUCCEEDED(hr) &&
      SUCCEEDED(hr = ::CreateURLMonikerEx(NULL, url_.c_str(), moniker.Receive(),
                                          URL_MK_UNIFORM))) {
    DCHECK(SUCCEEDED(hr));
    if (SUCCEEDED(hr)) {
#ifndef NDEBUG
      // We've just created a new moniker for a URL that has just been fetched.
      // However, the moniker used for the current navigation should still
      // be running.
      // The documentation for IMoniker::IsRunning() states that if the
      // moniker is already running (i.e. an equal moniker already exists),
      // then the return value from IsRunning will be S_OK (or 0) and
      // S_FALSE (1) when the moniker is not running.
      // http://msdn.microsoft.com/en-us/library/ms678475(VS.85).aspx
      // However, knowing that the IsRunning implementation relies on
      // the bind context and that the bind context uses ole32's
      // IRunningObjectTable::IsRunning to do its bidding, the return value
      // is actually TRUE (or 1) when the moniker is running and FALSE (0)
      // when it is not running.  Yup, the opposite of what you'd expect :-)
      // http://msdn.microsoft.com/en-us/library/ms682169(VS.85).aspx
      HRESULT running = moniker->IsRunning(bind_context, NULL, NULL);
      DCHECK(running == TRUE) << "Moniker not already running?";
#endif

      // If there's a referrer, preserve it.
      std::wstring headers;
      if (!referrer_.empty()) {
        headers = StringPrintf(L"Referer: %ls\r\n\r\n",
            ASCIIToWide(referrer_).c_str());
      }

      // Pass in URL fragments if applicable.
      std::wstring fragment;
      GURL parsed_moniker_url(url_);
      if (parsed_moniker_url.has_ref()) {
        fragment = UTF8ToWide(parsed_moniker_url.ref());
      }

      hr = NavigateBrowserToMoniker(browser, moniker, headers.c_str(),
          bind_context, fragment.c_str());
    }
  }

  return hr;
}

namespace {

// See comments in Bho::OnHttpEquiv for details.
void ClearDocumentContents(IUnknown* browser) {
  ScopedComPtr<IWebBrowser2> web_browser2;
  if (SUCCEEDED(DoQueryService(SID_SWebBrowserApp, browser,
                               web_browser2.Receive()))) {
    ScopedComPtr<IDispatch> doc_disp;
    web_browser2->get_Document(doc_disp.Receive());
    ScopedComPtr<IHTMLDocument2> doc;
    if (doc_disp && SUCCEEDED(doc.QueryFrom(doc_disp))) {
      SAFEARRAY* sa = ::SafeArrayCreateVector(VT_UI1, 0, 0);
      doc->write(sa);
      ::SafeArrayDestroy(sa);
    }
  }
}

// Returns true if the currently loaded document in the browser has
// any embedded items such as a frame or an iframe.
bool DocumentHasEmbeddedItems(IUnknown* browser) {
  bool has_embedded_items = false;

  ScopedComPtr<IWebBrowser2> web_browser2;
  ScopedComPtr<IDispatch> document;
  if (SUCCEEDED(DoQueryService(SID_SWebBrowserApp, browser,
                               web_browser2.Receive())) &&
      SUCCEEDED(web_browser2->get_Document(document.Receive()))) {
    ScopedComPtr<IOleContainer> container;
    if (SUCCEEDED(container.QueryFrom(document))) {
      ScopedComPtr<IEnumUnknown> enumerator;
      container->EnumObjects(OLECONTF_EMBEDDINGS, enumerator.Receive());
      if (enumerator) {
        ScopedComPtr<IUnknown> unk;
        DWORD fetched = 0;
        enumerator->Next(1, unk.Receive(), &fetched);
        has_embedded_items = (fetched != 0);
      }
    }
  }

  return has_embedded_items;
}

}  // end namespace

HRESULT Bho::OnHttpEquiv(IBrowserService_OnHttpEquiv_Fn original_httpequiv,
    IBrowserService* browser, IShellView* shell_view, BOOL done,
    VARIANT* in_arg, VARIANT* out_arg) {
  DLOG(INFO) << __FUNCTION__ << " done:" << done;

  // OnHttpEquiv with 'done' set to TRUE is called for all pages.
  // 0 or more calls with done set to FALSE are made.
  // When done is FALSE, the current moniker may not represent the page
  // being navigated to so we always have to wait for done to be TRUE
  // before re-initiating the navigation.

  if (!done && in_arg && VT_BSTR == V_VT(in_arg)) {
    if (StrStrI(V_BSTR(in_arg), kChromeContentPrefix)) {
      // OnHttpEquiv is invoked for meta tags within sub frames as well.
      // We want to switch renderers only for the top level frame.
      // The theory here is that if there are any existing embedded items
      // (frames or iframes) in the current document, then the http-equiv
      // notification is coming from those and not the top level document.
      // The embedded items should only be created once the top level
      // doc has been created.
      if (!DocumentHasEmbeddedItems(browser)) {
        Bho* bho = Bho::GetCurrentThreadBhoInstance();
        DCHECK(bho);
        DLOG(INFO) << "Found tag in page. Marking browser." << bho->url() <<
            StringPrintf(" tid=0x%08X", ::GetCurrentThreadId());
        if (bho) {
          // TODO(tommi): See if we can't figure out a cleaner way to avoid
          // this.  For some documents we can hit a problem here.  When we
          // attempt to navigate the document again in CF, mshtml can "complete"
          // the current navigation (if all data is available) and fire off
          // script events such as onload and even render the page.
          // This will happen inside NavigateBrowserToMoniker below.
          // To work around this, we clear the contents of the document before
          // opening it up in CF.
          ClearDocumentContents(browser);
          bho->NavigateToCurrentUrlInCF(browser);
        }
      }
    }
  }

  return original_httpequiv(browser, shell_view, done, in_arg, out_arg);
}

void Bho::OnBeginningTransaction(IWebBrowser2* browser, const wchar_t* url,
                                 const wchar_t* headers,
                                 const wchar_t* additional_headers) {
  if (!browser)
    return;

  if (url_.compare(url) != 0) {
    DLOG(INFO) << __FUNCTION__ << " not processing headers for url: " << url
        << " current url is: " << url_;
    return;
  }

  VARIANT_BOOL is_top_level = VARIANT_FALSE;
  browser->get_TopLevelContainer(&is_top_level);
  if (is_top_level) {
    ScopedComPtr<IBrowserService> browser_service;
    DoQueryService(SID_SShellBrowser, browser, browser_service.Receive());
    if (!browser_service || !CheckForCFNavigation(browser_service, false)) {
      // Save away the referrer in case our active document needs it to initiate
      // navigation in chrome.
      referrer_.clear();
      const wchar_t* both_headers[] = { headers, additional_headers };
      for (int i = 0; referrer_.empty() && i < arraysize(both_headers); ++i) {
        if (!both_headers[i])
          continue;
        std::string raw_headers_utf8 = WideToUTF8(both_headers[i]);
        std::string http_headers =
            net::HttpUtil::AssembleRawHeaders(raw_headers_utf8.c_str(),
                                              raw_headers_utf8.length());
        net::HttpUtil::HeadersIterator it(http_headers.begin(),
                                          http_headers.end(), "\r\n");
        while (it.GetNext()) {
          if (LowerCaseEqualsASCII(it.name(), "referer")) {
            referrer_ = it.values();
            break;
          }
        }
      }
    }
  }
}

Bho* Bho::GetCurrentThreadBhoInstance() {
  DCHECK(bho_current_thread_instance_.Pointer()->Get() != NULL);
  return bho_current_thread_instance_.Pointer()->Get();
}

// static
void Bho::ProcessOptInUrls(IWebBrowser2* browser, BSTR url) {
  if (!browser || !url) {
    NOTREACHED();
    return;
  }

  VARIANT_BOOL is_top_level = VARIANT_FALSE;
  browser->get_TopLevelContainer(&is_top_level);
  if (is_top_level) {
    std::wstring current_url(url, SysStringLen(url));
    if (IsValidUrlScheme(current_url, false)) {
      bool cf_protocol = StartsWith(current_url, kChromeProtocolPrefix, false);
      if (!cf_protocol && IsOptInUrl(current_url.c_str())) {
        DLOG(INFO) << "Opt-in URL. Switching to cf.";
        ScopedComPtr<IBrowserService> browser_service;
        DoQueryService(SID_SShellBrowser, browser, browser_service.Receive());
        DCHECK(browser_service) << "DoQueryService - SID_SShellBrowser failed.";
        MarkBrowserOnThreadForCFNavigation(browser_service);
      }
    }
  }
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

    bool patch_protocol = GetConfigBool(false, kPatchProtocols);
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
  if (!IS_PATCHED(IBrowserService)) {
    vtable_patch::PatchInterfaceMethods(browser_service,
                                        IBrowserService_PatchInfo);
  }
}

void PatchHelper::UnpatchIfNeeded() {
  if (state_ == PATCH_PROTOCOL) {
    ProtocolSinkWrap::UnpatchProtocolHandlers();
  } else if (state_ == PATCH_IBROWSER) {
    vtable_patch::UnpatchInterfaceMethods(IBrowserService_PatchInfo);
  }

  HttpNegotiatePatch::Uninitialize();

  state_ = UNKNOWN;
}

