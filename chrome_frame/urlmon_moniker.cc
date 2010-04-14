// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_moniker.h"

#include <shlguid.h>

#include "base/string_util.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/chrome_active_document.h"
#include "chrome_frame/urlmon_bind_status_callback.h"
#include "chrome_frame/vtable_patch_manager.h"
#include "net/http/http_util.h"

static const int kMonikerBindToObject = 8;
static const int kMonikerBindToStorage = kMonikerBindToObject + 1;

// These are non const due to API expectations
static wchar_t* kBindContextCachedData = L"_CHROMEFRAME_PRECREATE_";
static wchar_t* kBindToObjectBind = L"_CHROMEFRAME_BTO_BIND_";
wchar_t* kChromeRequestParam = L"_CHROMEFRAME_REQUEST_";


base::LazyInstance<base::ThreadLocalPointer<NavigationManager> >
    NavigationManager::thread_singleton_(base::LINKER_INITIALIZED);

BEGIN_VTABLE_PATCHES(IMoniker)
  VTABLE_PATCH_ENTRY(kMonikerBindToObject, MonikerPatch::BindToObject)
  VTABLE_PATCH_ENTRY(kMonikerBindToStorage, MonikerPatch::BindToStorage)
END_VTABLE_PATCHES()

namespace {
std::string FindReferrerFromHeaders(const wchar_t* headers,
                                     const wchar_t* additional_headers) {
  std::string referrer;

  const wchar_t* both_headers[] = { headers, additional_headers };
  for (int i = 0; referrer.empty() && i < arraysize(both_headers); ++i) {
    if (!both_headers[i])
      continue;
    std::string raw_headers_utf8 = WideToUTF8(both_headers[i]);
    net::HttpUtil::HeadersIterator it(raw_headers_utf8.begin(),
                                      raw_headers_utf8.end(), "\r\n");
    while (it.GetNext()) {
      if (LowerCaseEqualsASCII(it.name(), "referer")) {
        referrer = it.values();
        break;
      }
    }
  }

  return referrer;
}

}  // end namespace


////////////////////////////

HRESULT NavigationManager::NavigateToCurrentUrlInCF(IBrowserService* browser) {
  DCHECK(browser);
  DLOG(INFO) << __FUNCTION__ << " " << url();

  MarkBrowserOnThreadForCFNavigation(browser);

  HRESULT hr = S_OK;
  ScopedComPtr<IShellBrowser> shell_browser;
  ScopedComPtr<IBindCtx> bind_context;
  hr = ::CreateAsyncBindCtxEx(NULL, 0, NULL, NULL, bind_context.Receive(), 0);

  ScopedComPtr<IMoniker> moniker;
  DCHECK(bind_context);
  if (SUCCEEDED(hr) &&
      SUCCEEDED(hr = ::CreateURLMonikerEx(NULL, url_.c_str(), moniker.Receive(),
                                          URL_MK_UNIFORM))) {
    if (SUCCEEDED(hr)) {
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
      DLOG(INFO) << StringPrintf("NavigateBrowserToMoniker: 0x%08X", hr);
    }
  }

  return hr;
}

bool NavigationManager::IsTopLevelUrl(const wchar_t* url) {
  return CompareUrlsWithoutFragment(url_.c_str(), url);
}

void NavigationManager::OnBeginningTransaction(bool is_top_level,
    const wchar_t* url, const wchar_t* headers,
    const wchar_t* additional_headers) {
  DCHECK(url != NULL);

  // We've seen this happen on the first page load in IE8 (it might also happen
  // in other versions of IE) and for some reason we did not get a call to
  // BeginningTransaction the first time it happened and this time around we're
  // using a cached data object that didn't get the request headers or URL.
  DLOG_IF(ERROR, lstrlenW(url) == 0) << __FUNCTION__ << "Invalid URL.";

  if (!is_top_level)
    return;

  if (url_.compare(url) != 0) {
    DLOG(INFO) << __FUNCTION__ << " not processing headers for url: " << url
        << " current url is: " << url_;
    return;
  }

  // Save away the referrer in case our active document needs it to initiate
  // navigation in chrome.
  referrer_ = FindReferrerFromHeaders(headers, additional_headers);
}

/////////////////////////////////////////

NavigationManager* NavigationManager::GetThreadInstance() {
  return thread_singleton_.Pointer()->Get();
}

void NavigationManager::RegisterThreadInstance() {
  DCHECK(GetThreadInstance() == NULL);
  thread_singleton_.Pointer()->Set(this);
}

void NavigationManager::UnregisterThreadInstance() {
  DCHECK(GetThreadInstance() == this);
  thread_singleton_.Pointer()->Set(NULL);
}

// Mark a bind context for navigation by storing a bind context param.
bool NavigationManager::SetForSwitch(IBindCtx* bind_context, IStream* data) {
  if (!bind_context) {
    NOTREACHED();
    return false;
  }

  RewindStream(data);
  HRESULT hr = bind_context->RegisterObjectParam(kBindContextCachedData, data);
  return SUCCEEDED(hr);
}

bool NavigationManager::IsSetToSwitch(IBindCtx* bind_context) {
  if (!bind_context) {
    NOTREACHED();
    return false;
  }

  ScopedComPtr<IUnknown> should_switch;
  HRESULT hr = E_FAIL;
  hr = bind_context->GetObjectParam(kBindContextCachedData,
                                    should_switch.Receive());
  return !!should_switch;
}

HRESULT NavigationManager::ResetSwitch(IBindCtx* bind_context, IStream** data) {
  if (!bind_context) {
    NOTREACHED();
    return false;
  }

  ScopedComPtr<IUnknown> data_unknown;
  HRESULT hr = E_FAIL;
  hr = bind_context->GetObjectParam(kBindContextCachedData,
                                    data_unknown.Receive());
  hr = bind_context->RevokeObjectParam(kBindContextCachedData);
  if (data_unknown) {
    hr = data_unknown.QueryInterface(data);
    DCHECK(SUCCEEDED(hr));
  }
  return hr;
}

/////////////////////////////////////////

// static
bool MonikerPatch::Initialize() {
  if (IS_PATCHED(IMoniker)) {
    DLOG(WARNING) << __FUNCTION__ << " called more than once.";
    return true;
  }

  ScopedComPtr<IMoniker> moniker;
  HRESULT hr = ::CreateURLMoniker(NULL, L"http://localhost/",
                                  moniker.Receive());
  DCHECK(SUCCEEDED(hr));
  if (SUCCEEDED(hr)) {
    hr = vtable_patch::PatchInterfaceMethods(moniker, IMoniker_PatchInfo);
    DLOG_IF(ERROR, FAILED(hr)) << StringPrintf("patch failed 0x%08X", hr);
  }

  return SUCCEEDED(hr);
}

// static
void MonikerPatch::Uninitialize() {
  vtable_patch::UnpatchInterfaceMethods(IMoniker_PatchInfo);
}

bool ShouldWrapCallback(IMoniker* moniker, REFIID iid, IBindCtx* bind_context) {
  CComHeapPtr<WCHAR> url;
  HRESULT hr = moniker->GetDisplayName(bind_context, NULL, &url);
  if (!url) {
    DLOG(INFO) << __FUNCTION__ << StringPrintf(
        " GetDisplayName failed. Error: 0x%x", hr);
    return false;
  }

  if (!IsEqualIID(IID_IStream, iid)) {
    DLOG(INFO) << __FUNCTION__ << " Url: " << url <<
        " Not wrapping: IID is not IStream.";
    return false;
  }

  ScopedComPtr<IUnknown> our_request;
  hr = bind_context->GetObjectParam(kChromeRequestParam, our_request.Receive());
  if (our_request) {
    DLOG(INFO) << __FUNCTION__ << " Url: " << url <<
        " Not wrapping: request from chrome frame.";
    return false;
  }

  NavigationManager* mgr = NavigationManager::GetThreadInstance();
  if (!mgr) {
    DLOG(INFO) << __FUNCTION__ << " Url: " << url <<
        " No navitagion manager to wrap";
    return false;
  }

  bool should_wrap = mgr->IsTopLevelUrl(url);
  if (!should_wrap) {
    DLOG(INFO) << __FUNCTION__ << " Url: " << url <<
        " Not wrapping: Not top level url.";
  }

  return should_wrap;
}

// static
HRESULT MonikerPatch::BindToObject(IMoniker_BindToObject_Fn original,
                                   IMoniker* me, IBindCtx* bind_ctx,
                                   IMoniker* to_left, REFIID iid, void** obj) {
  DLOG(INFO) << __FUNCTION__;
  DCHECK(to_left == NULL);

  HRESULT hr = S_OK;
  // Bind context is marked for switch when we sniff data in BSCBStorageBind
  // and determine that the renderer to be used is Chrome.
  if (NavigationManager::IsSetToSwitch(bind_ctx)) {
    // We could implement the BindToObject ourselves here but instead we
    // simply register Chrome Frame ActiveDoc as a handler for 'text/html'
    // in this bind context.  This makes urlmon instantiate CF Active doc
    // instead of mshtml.
    char* media_types[] = { "text/html" };
    CLSID classes[] = { CLSID_ChromeActiveDocument };
    hr = RegisterMediaTypeClass(bind_ctx, arraysize(media_types), media_types,
                                classes, 0);
  } else {
    // In case the binding begins with BindToObject we do not need
    // to cache the data in the sniffing code.
    ScopedComPtr<IStream> no_cache;
    CreateStreamOnHGlobal(NULL, TRUE, no_cache.Receive());
    if (no_cache)
      bind_ctx->RegisterObjectParam(kBindToObjectBind, no_cache);
  }

  hr = original(me, bind_ctx, to_left, iid, obj);
  return hr;
}

// static
HRESULT MonikerPatch::BindToStorage(IMoniker_BindToStorage_Fn original,
                                    IMoniker* me, IBindCtx* bind_ctx,
                                    IMoniker* to_left, REFIID iid, void** obj) {
  DCHECK(to_left == NULL);

  HRESULT hr = S_OK;
  CComObject<BSCBStorageBind>* callback = NULL;
  if (ShouldWrapCallback(me, iid, bind_ctx)) {
    // Is this bind context marked as no cache by BindToObject already?
    ScopedComPtr<IUnknown> no_cache;
    if (bind_ctx)
      bind_ctx->GetObjectParam(kBindToObjectBind, no_cache.Receive());

    hr = CComObject<BSCBStorageBind>::CreateInstance(&callback);
    callback->AddRef();
    hr = callback->Initialize(me, bind_ctx, !!no_cache);
    DCHECK(SUCCEEDED(hr));
  }

  hr = original(me, bind_ctx, to_left, iid, obj);

  // If the binding terminates before the data could be played back
  // now is the chance. Sometimes OnStopBinding happens after this returns
  // and then it's too late.
  if ((S_OK == hr) && callback)
    callback->MayPlayBack(BSCF_LASTDATANOTIFICATION);
  return hr;
}

