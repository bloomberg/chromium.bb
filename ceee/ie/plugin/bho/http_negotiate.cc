// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ceee/ie/plugin/bho/http_negotiate.h"

#include <atlbase.h>
#include <atlctl.h>
#include <htiframe.h>
#include <urlmon.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"

#include "ceee/ie/plugin/bho/cookie_accountant.h"
#include "chrome_frame/vtable_patch_manager.h"
#include "chrome_frame/utils.h"
#include "base/scoped_comptr_win.h"

static const int kHttpNegotiateBeginningTransactionIndex = 3;
static const int kHttpNegotiateOnResponseIndex = 4;

CComAutoCriticalSection HttpNegotiatePatch::bho_instance_count_crit_;
int HttpNegotiatePatch::bho_instance_count_ = 0;


BEGIN_VTABLE_PATCHES(IHttpNegotiate)
  VTABLE_PATCH_ENTRY(kHttpNegotiateBeginningTransactionIndex,
                     HttpNegotiatePatch::BeginningTransaction)
  VTABLE_PATCH_ENTRY(kHttpNegotiateOnResponseIndex,
                     HttpNegotiatePatch::OnResponse)
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

HttpNegotiatePatch::HttpNegotiatePatch() {
}

HttpNegotiatePatch::~HttpNegotiatePatch() {
}

// static
bool HttpNegotiatePatch::Initialize() {
  // Patch IHttpNegotiate for user-agent and cookie functionality.
  {
    CComCritSecLock<CComAutoCriticalSection> lock(bho_instance_count_crit_);
    bho_instance_count_++;
    if (bho_instance_count_ != 1) {
      return true;
    }
  }

  if (IS_PATCHED(IHttpNegotiate)) {
    LOG(WARNING) << __FUNCTION__ << ": already patched.";
    return true;
  }

  // Use our SimpleBindStatusCallback class as we need a temporary object that
  // implements IBindStatusCallback.
  CComObjectStackEx<SimpleBindStatusCallback> request;
  ScopedComPtr<IBindCtx> bind_ctx;
  HRESULT hr = ::CreateAsyncBindCtx(0, &request, NULL, bind_ctx.Receive());

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
  CComCritSecLock<CComAutoCriticalSection> lock(bho_instance_count_crit_);
  bho_instance_count_--;
  DCHECK_GE(bho_instance_count_, 0);
  if (bho_instance_count_ == 0) {
    vtable_patch::UnpatchInterfaceMethods(IHttpNegotiate_PatchInfo);
  }
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
        << StringPrintf("HttpNegotiate patch failed 0x%08X", hr);
  } else {
    DLOG(WARNING)
        << StringPrintf("IHttpNegotiate not supported 0x%08X", hr);
  }

  return hr;
}


// static
HRESULT HttpNegotiatePatch::BeginningTransaction(
    IHttpNegotiate_BeginningTransaction_Fn original, IHttpNegotiate* me,
    LPCWSTR url, LPCWSTR headers, DWORD reserved, LPWSTR* additional_headers) {
  DLOG(INFO) << __FUNCTION__ << " " << url << " headers:\n" << headers;

  HRESULT hr = original(me, url, headers, reserved, additional_headers);

  if (FAILED(hr)) {
    DLOG(WARNING) << __FUNCTION__ << " Delegate returned an error";
    return hr;
  }

  // TODO(skare@google.com): Modify User-Agent here.

  return hr;
}

// static
HRESULT HttpNegotiatePatch::OnResponse(
    IHttpNegotiate_OnResponse_Fn original, IHttpNegotiate* me,
    DWORD response_code, LPCWSTR response_headers, LPCWSTR request_headers,
    LPWSTR* additional_request_headers) {
  DLOG(INFO) << __FUNCTION__ << " response headers:\n" << response_headers;

  base::Time current_time = base::Time::Now();

  HRESULT hr = original(me, response_code, response_headers, request_headers,
      additional_request_headers);

  if (FAILED(hr)) {
    DLOG(WARNING) << __FUNCTION__ << " Delegate returned an error";
    return hr;
  }

  CookieAccountant::GetInstance()->RecordHttpResponseCookies(
    std::string(CW2A(response_headers)), current_time);

  return hr;
}
