// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/http_negotiate.h"

#include <atlbase.h>
#include <atlcom.h>
#include <htiframe.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome_frame/bho.h"
#include "chrome_frame/exception_barrier.h"
#include "chrome_frame/html_utils.h"
#include "chrome_frame/urlmon_url_request.h"
#include "chrome_frame/urlmon_moniker.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/vtable_patch_manager.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

bool HttpNegotiatePatch::modify_user_agent_ = true;
const char kUACompatibleHttpHeader[] = "x-ua-compatible";
const char kLowerCaseUserAgent[] = "user-agent";

// From the latest urlmon.h. Symbol name prepended with LOCAL_ to
// avoid conflict (and therefore build errors) for those building with
// a newer Windows SDK.
// TODO(robertshield): Remove this once we update our SDK version.
const int LOCAL_BINDSTATUS_SERVER_MIMETYPEAVAILABLE = 54;

static const int kHttpNegotiateBeginningTransactionIndex = 3;

BEGIN_VTABLE_PATCHES(IHttpNegotiate)
  VTABLE_PATCH_ENTRY(kHttpNegotiateBeginningTransactionIndex,
                     HttpNegotiatePatch::BeginningTransaction)
END_VTABLE_PATCHES()

namespace {

class SimpleBindStatusCallback : public CComObjectRootEx<CComSingleThreadModel>,
                                 public IBindStatusCallback {
 public:
  BEGIN_COM_MAP(SimpleBindStatusCallback)
    COM_INTERFACE_ENTRY(IBindStatusCallback)
  END_COM_MAP()

  // IBindStatusCallback implementation
  STDMETHOD(OnStartBinding)(DWORD reserved, IBinding* binding) {
    return E_NOTIMPL;
  }

  STDMETHOD(GetPriority)(LONG* priority) {
    return E_NOTIMPL;
  }
  STDMETHOD(OnLowResource)(DWORD reserved) {
    return E_NOTIMPL;
  }

  STDMETHOD(OnProgress)(ULONG progress, ULONG max_progress,
                        ULONG status_code, LPCWSTR status_text) {
    return E_NOTIMPL;
  }
  STDMETHOD(OnStopBinding)(HRESULT result, LPCWSTR error) {
    return E_NOTIMPL;
  }

  STDMETHOD(GetBindInfo)(DWORD* bind_flags, BINDINFO* bind_info) {
    return E_NOTIMPL;
  }

  STDMETHOD(OnDataAvailable)(DWORD flags, DWORD size, FORMATETC* formatetc,
    STGMEDIUM* storage) {
    return E_NOTIMPL;
  }
  STDMETHOD(OnObjectAvailable)(REFIID iid, IUnknown* object) {
    return E_NOTIMPL;
  }
};
}  // end namespace

std::string AppendCFUserAgentString(LPCWSTR headers,
                                    LPCWSTR additional_headers) {
  using net::HttpUtil;

  std::string ascii_headers;
  if (additional_headers) {
    ascii_headers = WideToASCII(additional_headers);
  }

  // Extract "User-Agent" from |additional_headers| or |headers|.
  HttpUtil::HeadersIterator headers_iterator(ascii_headers.begin(),
                                             ascii_headers.end(), "\r\n");
  std::string user_agent_value;
  if (headers_iterator.AdvanceTo(kLowerCaseUserAgent)) {
    user_agent_value = headers_iterator.values();
  } else if (headers != NULL) {
    // See if there's a user-agent header specified in the original headers.
    std::string original_headers(WideToASCII(headers));
    HttpUtil::HeadersIterator original_it(original_headers.begin(),
        original_headers.end(), "\r\n");
    if (original_it.AdvanceTo(kLowerCaseUserAgent))
      user_agent_value = original_it.values();
  }

  // Use the default "User-Agent" if none was provided.
  if (user_agent_value.empty())
    user_agent_value = http_utils::GetDefaultUserAgent();

  // Now add chromeframe to it.
  user_agent_value = http_utils::AddChromeFrameToUserAgentValue(
      user_agent_value);

  // Build new headers, skip the existing user agent value from
  // existing headers.
  std::string new_headers;
  headers_iterator.Reset();
  while (headers_iterator.GetNext()) {
    std::string name(headers_iterator.name());
    if (!LowerCaseEqualsASCII(name, kLowerCaseUserAgent)) {
      new_headers += name + ": " + headers_iterator.values() + "\r\n";
    }
  }

  new_headers += "User-Agent: " + user_agent_value;
  new_headers += "\r\n";
  return new_headers;
}

std::string ReplaceOrAddUserAgent(LPCWSTR headers,
                                  const std::string& user_agent_value) {
  using net::HttpUtil;

  std::string new_headers;
  if (headers) {
    std::string ascii_headers(WideToASCII(headers));

    // Extract "User-Agent" from the headers.
    HttpUtil::HeadersIterator headers_iterator(ascii_headers.begin(),
                                               ascii_headers.end(), "\r\n");

    // Build new headers, skip the existing user agent value from
    // existing headers.
    while (headers_iterator.GetNext()) {
      std::string name(headers_iterator.name());
      if (!LowerCaseEqualsASCII(name, kLowerCaseUserAgent)) {
        new_headers += name + ": " + headers_iterator.values() + "\r\n";
      }
    }
  }
  new_headers += "User-Agent: " + user_agent_value;
  new_headers += "\r\n";
  return new_headers;
}

HttpNegotiatePatch::HttpNegotiatePatch() {
}

HttpNegotiatePatch::~HttpNegotiatePatch() {
}

// static
bool HttpNegotiatePatch::Initialize() {
  if (IS_PATCHED(IHttpNegotiate)) {
    DLOG(WARNING) << __FUNCTION__ << " called more than once.";
    return true;
  }
  // Use our SimpleBindStatusCallback class as we need a temporary object that
  // implements IBindStatusCallback.
  CComObjectStackEx<SimpleBindStatusCallback> request;
  ScopedComPtr<IBindCtx> bind_ctx;
  HRESULT hr = CreateAsyncBindCtx(0, &request, NULL, bind_ctx.Receive());
  DCHECK(SUCCEEDED(hr)) << "CreateAsyncBindCtx";
  if (bind_ctx) {
    ScopedComPtr<IUnknown> bscb_holder;
    bind_ctx->GetObjectParam(L"_BSCB_Holder_", bscb_holder.Receive());
    if (bscb_holder) {
      hr = PatchHttpNegotiate(bscb_holder);
    } else {
      NOTREACHED() << "Failed to get _BSCB_Holder_";
      hr = E_UNEXPECTED;
    }
    bind_ctx.Release();
  }

  return SUCCEEDED(hr);
}

// static
void HttpNegotiatePatch::Uninitialize() {
  vtable_patch::UnpatchInterfaceMethods(IHttpNegotiate_PatchInfo);
}

// static
HRESULT HttpNegotiatePatch::PatchHttpNegotiate(IUnknown* to_patch) {
  DCHECK(to_patch);
  DCHECK_IS_NOT_PATCHED(IHttpNegotiate);

  ScopedComPtr<IHttpNegotiate> http;
  HRESULT hr = http.QueryFrom(to_patch);
  if (FAILED(hr)) {
    hr = DoQueryService(IID_IHttpNegotiate, to_patch, http.Receive());
  }

  if (http) {
    hr = vtable_patch::PatchInterfaceMethods(http, IHttpNegotiate_PatchInfo);
    DLOG_IF(ERROR, FAILED(hr))
        << base::StringPrintf("HttpNegotiate patch failed 0x%08X", hr);
  } else {
    DLOG(WARNING)
        << base::StringPrintf("IHttpNegotiate not supported 0x%08X", hr);
  }
  return hr;
}

// static
HRESULT HttpNegotiatePatch::BeginningTransaction(
    IHttpNegotiate_BeginningTransaction_Fn original, IHttpNegotiate* me,
    LPCWSTR url, LPCWSTR headers, DWORD reserved, LPWSTR* additional_headers) {
  DVLOG(1) << __FUNCTION__ << " " << url << " headers:\n" << headers;

  HRESULT hr = original(me, url, headers, reserved, additional_headers);

  if (FAILED(hr)) {
    DLOG(WARNING) << __FUNCTION__ << " Delegate returned an error";
    return hr;
  }
  if (modify_user_agent_) {
    std::string updated_headers;
    if (IsGcfDefaultRenderer() &&
        RendererTypeForUrl(url) == RENDERER_TYPE_CHROME_DEFAULT_RENDERER) {
      // Replace the user-agent header with Chrome's.
      updated_headers = ReplaceOrAddUserAgent(*additional_headers,
                                              http_utils::GetChromeUserAgent());
    } else {
      updated_headers = AppendCFUserAgentString(headers, *additional_headers);
    }
    *additional_headers = reinterpret_cast<wchar_t*>(::CoTaskMemRealloc(
        *additional_headers,
        (updated_headers.length() + 1) * sizeof(wchar_t)));
    lstrcpyW(*additional_headers, ASCIIToWide(updated_headers).c_str());
  }
  return S_OK;
}
