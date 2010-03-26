// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/urlmon_bind_status_callback.h"

#include <shlguid.h>

#include "base/logging.h"
#include "base/string_util.h"

CFUrlmonBindStatusCallback::CFUrlmonBindStatusCallback()
    : only_buffer_(false), data_(new RequestData()) {
  DLOG(INFO) << __FUNCTION__ << me();
}

CFUrlmonBindStatusCallback::~CFUrlmonBindStatusCallback() {
  DLOG(INFO) << __FUNCTION__ << me();
}

std::string CFUrlmonBindStatusCallback::me() {
  return StringPrintf(" obj=0x%08X",
      static_cast<CFUrlmonBindStatusCallback*>(this));
}

HRESULT CFUrlmonBindStatusCallback::DelegateQI(void* obj, REFIID iid,
                                               void** ret, DWORD cookie) {
  CFUrlmonBindStatusCallback* me =
      reinterpret_cast<CFUrlmonBindStatusCallback*>(obj);
  HRESULT hr = me->delegate_.QueryInterface(iid, ret);
  return hr;
}

HRESULT CFUrlmonBindStatusCallback::Initialize(IBindCtx* bind_ctx,
                                               RequestHeaders* headers) {
  DLOG(INFO) << __FUNCTION__ << me();
  DCHECK(bind_ctx);
  DCHECK(!binding_delegate_.get());
  // headers may be NULL.

  data_->Initialize(headers);

  bind_ctx_ = bind_ctx;

  // Replace the bind context callback with ours.
  HRESULT hr = ::RegisterBindStatusCallback(bind_ctx, this,
                                            delegate_.Receive(), 0);
  if (!delegate_) {
    NOTREACHED();
    hr = E_UNEXPECTED;
  }

  return hr;
}

HRESULT CFUrlmonBindStatusCallback::QueryService(REFGUID service, REFIID iid,
                                                 void** object) {
  HRESULT hr = E_NOINTERFACE;
  if (delegate_) {
    ScopedComPtr<IServiceProvider> svc;
    svc.QueryFrom(delegate_);
    if (svc) {
      hr = svc->QueryService(service, iid, object);
    }
  }
  return hr;
}

// IBindStatusCallback
HRESULT CFUrlmonBindStatusCallback::OnStartBinding(DWORD reserved,
                                                   IBinding* binding) {
  DLOG(INFO) << __FUNCTION__ << me() << StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  DCHECK(!binding_delegate_.get());

  CComObject<SimpleBindingImpl>* binding_delegate;
  HRESULT hr = CComObject<SimpleBindingImpl>::CreateInstance(&binding_delegate);
  if (FAILED(hr)) {
    NOTREACHED();
    return hr;
  }

  binding_delegate_ = binding_delegate;
  DCHECK_EQ(binding_delegate->m_dwRef, 1);
  binding_delegate_->SetDelegate(binding);

  return delegate_->OnStartBinding(reserved, binding_delegate_);
}

HRESULT CFUrlmonBindStatusCallback::GetPriority(LONG* priority) {
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  return delegate_->GetPriority(priority);
}

HRESULT CFUrlmonBindStatusCallback::OnLowResource(DWORD reserved) {
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  return delegate_->OnLowResource(reserved);
}

HRESULT CFUrlmonBindStatusCallback::OnProgress(ULONG progress,
                                               ULONG progress_max,
                                               ULONG status_code,
                                               LPCWSTR status_text) {
  DLOG(INFO) << __FUNCTION__ << me() << StringPrintf(" status=%i tid=%i %ls",
      status_code, PlatformThread::CurrentId(), status_text);
  if (status_code == BINDSTATUS_REDIRECTING && status_text) {
    redirected_url_ = status_text;
  }
  return delegate_->OnProgress(progress, progress_max, status_code,
                               status_text);
}

HRESULT CFUrlmonBindStatusCallback::OnStopBinding(HRESULT hresult,
                                                  LPCWSTR error) {
  DLOG(INFO) << __FUNCTION__ << me() << StringPrintf(" hr=0x%08X '%ls' tid=%i",
      hresult, error, PlatformThread::CurrentId());
  if (SUCCEEDED(hresult)) {
    // Notify the BHO that this is the one and only RequestData object.
    NavigationManager* mgr = NavigationManager::GetThreadInstance();
    DCHECK(mgr);
    if (mgr && data_->GetCachedContentSize()) {
      mgr->SetActiveRequestData(data_);
      if (!redirected_url_.empty()) {
        mgr->set_url(redirected_url_.c_str());
      }
    }

    if (only_buffer_) {
      hresult = INET_E_TERMINATED_BIND;
      DLOG(INFO) << " - changed to INET_E_TERMINATED_BIND";
    }
  }

  // Hold a reference to ourselves while we release the bind context
  // and disconnect the callback.
  AddRef();

  HRESULT hr = delegate_->OnStopBinding(hresult, error);

  if (bind_ctx_) {
    ::RevokeBindStatusCallback(bind_ctx_, this);
    bind_ctx_.Release();
  }

  binding_delegate_.Release();

  // After this call, this object might be gone.
  Release();

  return hr;
}

HRESULT CFUrlmonBindStatusCallback::GetBindInfo(DWORD* bindf,
                                                BINDINFO* bind_info) {
  DLOG(INFO) << __FUNCTION__ << me() << StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  return delegate_->GetBindInfo(bindf, bind_info);
}

HRESULT CFUrlmonBindStatusCallback::OnDataAvailable(DWORD bscf, DWORD size,
                                                    FORMATETC* format_etc,
                                                    STGMEDIUM* stgmed) {
  DCHECK(format_etc);
#ifndef NDEBUG
  wchar_t clip_fmt_name[MAX_PATH] = {0};
  if (format_etc) {
    ::GetClipboardFormatNameW(format_etc->cfFormat, clip_fmt_name,
                              arraysize(clip_fmt_name));
  }
  DLOG(INFO) << __FUNCTION__ << me()
      << StringPrintf(" tid=%i original fmt=%ls",
                      PlatformThread::CurrentId(), clip_fmt_name);

  DCHECK(stgmed);
  DCHECK(stgmed->tymed == TYMED_ISTREAM);

  if (bscf & BSCF_FIRSTDATANOTIFICATION) {
    DLOG(INFO) << "first data notification";
  }
#endif

  HRESULT hr = S_OK;
  size_t bytes_read = 0;
  if (!only_buffer_) {
    hr = data_->DelegateDataRead(delegate_, bscf, size, format_etc, stgmed,
                                 &bytes_read);
  }

  DLOG(INFO) << __FUNCTION__ << StringPrintf(" - 0x%08x", hr);
  if (hr == INET_E_TERMINATED_BIND) {
    // Check if the content type is CF's mime type.
    UINT cf_format = ::RegisterClipboardFormatW(kChromeMimeType);
    bool override_bind_results = (format_etc->cfFormat == cf_format);
    if (!override_bind_results) {
      ScopedComPtr<IBrowserService> browser_service;
      DoQueryService(SID_SShellBrowser, delegate_, browser_service.Receive());
      override_bind_results = (browser_service != NULL) &&
                              CheckForCFNavigation(browser_service, false);
    }

    if (override_bind_results) {
      // We want to complete fetching the entire document even though the
      // delegate isn't interested in continuing.
      // This happens when we switch from mshtml to CF.
      // We take over and buffer the document and once we're done, we report
      // INET_E_TERMINATED to mshtml so that it will continue as usual.
      hr = S_OK;
      only_buffer_ = true;
      binding_delegate_->OverrideBindResults(INET_E_TERMINATED_BIND);
    }
  }

  if (only_buffer_) {
    data_->CacheAll(stgmed->pstm);
    DCHECK(hr == S_OK);
  }

  return hr;
}

HRESULT CFUrlmonBindStatusCallback::OnObjectAvailable(REFIID iid,
                                                      IUnknown* unk) {
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  return delegate_->OnObjectAvailable(iid, unk);
}

// IBindStatusCallbackEx
HRESULT CFUrlmonBindStatusCallback::GetBindInfoEx(DWORD* bindf,
                                                  BINDINFO* bind_info,
                                                  DWORD* bindf2,
                                                  DWORD* reserved) {
  DLOG(INFO) << __FUNCTION__ << StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  ScopedComPtr<IBindStatusCallbackEx> bscbex;
  bscbex.QueryFrom(delegate_);
  return bscbex->GetBindInfoEx(bindf, bind_info, bindf2, reserved);
}

HRESULT CFUrlmonBindStatusCallback::BeginningTransaction(LPCWSTR url,
    LPCWSTR headers,
    DWORD reserved,
    LPWSTR* additional_headers) {
  DLOG(INFO) << __FUNCTION__ << me() << StringPrintf(" tid=%i",
      PlatformThread::CurrentId());

  ScopedComPtr<IHttpNegotiate> http_negotiate;
  HRESULT hr = http_negotiate.QueryFrom(delegate_);
  if (SUCCEEDED(hr)) {
    hr = http_negotiate->BeginningTransaction(url, headers, reserved,
                                              additional_headers);
  } else {
    hr = S_OK;
  }

  data_->headers()->OnBeginningTransaction(url, headers,
      additional_headers && *additional_headers ? *additional_headers : NULL);

  DLOG_IF(ERROR, FAILED(hr)) << __FUNCTION__;
  return hr;
}

HRESULT CFUrlmonBindStatusCallback::OnResponse(DWORD response_code,
                                               LPCWSTR response_headers,
                                               LPCWSTR request_headers,
                                               LPWSTR* additional_headers) {
  DLOG(INFO) << __FUNCTION__ << me() << StringPrintf(" tid=%i",
      PlatformThread::CurrentId());

  data_->headers()->OnResponse(response_code, response_headers,
                               request_headers);

  ScopedComPtr<IHttpNegotiate> http_negotiate;
  HRESULT hr = http_negotiate.QueryFrom(delegate_);
  if (SUCCEEDED(hr)) {
    hr = http_negotiate->OnResponse(response_code, response_headers,
                                    request_headers, additional_headers);
  } else {
    hr = S_OK;
  }
  return hr;
}

HRESULT CFUrlmonBindStatusCallback::GetRootSecurityId(BYTE* security_id,
                                                      DWORD* security_id_size,
                                                      DWORD_PTR reserved) {
  ScopedComPtr<IHttpNegotiate2> http_negotiate;
  http_negotiate.QueryFrom(delegate_);
  return http_negotiate->GetRootSecurityId(security_id, security_id_size,
                                           reserved);
}

HRESULT CFUrlmonBindStatusCallback::GetSerializedClientCertContext(
    BYTE** cert,
    DWORD* cert_size) {
  ScopedComPtr<IHttpNegotiate3> http_negotiate;
  http_negotiate.QueryFrom(delegate_);
  return http_negotiate->GetSerializedClientCertContext(cert, cert_size);
}
