// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/bind_status_callback_impl.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

BSCBImpl::BSCBImpl() {
  DLOG(INFO) << __FUNCTION__ << me();
}

BSCBImpl::~BSCBImpl() {
  DLOG(INFO) << __FUNCTION__ << me();
}

std::string BSCBImpl::me() {
  return base::StringPrintf(" obj=0x%08X", static_cast<BSCBImpl*>(this));
}

HRESULT BSCBImpl::DelegateQI(void* obj, REFIID iid, void** ret, DWORD cookie) {
  BSCBImpl* me = reinterpret_cast<BSCBImpl*>(obj);
  HRESULT hr = E_NOINTERFACE;
  if (me->delegate_)
    hr = me->delegate_.QueryInterface(iid, ret);
  return hr;
}

void BSCBImpl::Initialize(IBindStatusCallback* original) {
  DCHECK(!delegate_);
  delegate_ = original;
}

HRESULT BSCBImpl::AttachToBind(IBindCtx* bind_ctx) {
  HRESULT hr = S_OK;
  hr = ::RegisterBindStatusCallback(bind_ctx, this, delegate_.Receive(), 0);
  if (SUCCEEDED(hr)) {
    bind_ctx_ = bind_ctx;
  }

  return hr;
}

HRESULT BSCBImpl::ReleaseBind() {
  // AddRef ourselves while we release these objects as we might
  // perish during this operation.
  AddRef();

  HRESULT hr = S_OK;
  if (bind_ctx_) {
    hr = ::RevokeBindStatusCallback(bind_ctx_, this);
  }
  delegate_.Release();
  bind_ctx_.Release();

  Release();

  return hr;
}

// IServiceProvider
HRESULT BSCBImpl::QueryService(REFGUID service, REFIID iid, void** object) {
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
HRESULT BSCBImpl::OnStartBinding(DWORD reserved, IBinding* binding) {
  DLOG(INFO) << __FUNCTION__ << me() << base::StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  HRESULT hr = S_OK;
  if (delegate_)
    hr = delegate_->OnStartBinding(reserved, binding);
  return hr;
}

HRESULT BSCBImpl::GetPriority(LONG* priority) {
  DLOG(INFO) << __FUNCTION__ << me() << base::StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  HRESULT hr = S_OK;
  if (delegate_)
    hr = delegate_->GetPriority(priority);
  return hr;
}

HRESULT BSCBImpl::OnLowResource(DWORD reserved) {
  DLOG(INFO) << __FUNCTION__ << me() << base::StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  HRESULT hr = S_OK;
  if (delegate_)
    hr = delegate_->OnLowResource(reserved);
  return hr;
}

HRESULT BSCBImpl::OnProgress(ULONG progress, ULONG progress_max,
                              ULONG status_code, LPCWSTR status_text) {
  DLOG(INFO) << __FUNCTION__ << me() << base::StringPrintf(
      " status=%i tid=%i %ls", status_code, PlatformThread::CurrentId(),
      status_text);
  HRESULT hr = S_OK;
  if (delegate_)
    delegate_->OnProgress(progress, progress_max, status_code, status_text);
  return hr;
}

HRESULT BSCBImpl::OnStopBinding(HRESULT hresult, LPCWSTR error) {
  DLOG(INFO) << __FUNCTION__ << me() << base::StringPrintf(
      " hr=0x%08X '%ls' tid=%i", hresult, error, PlatformThread::CurrentId());
  HRESULT hr = S_OK;
  if (delegate_)
    delegate_->OnStopBinding(hresult, error);
  return hr;
}

HRESULT BSCBImpl::GetBindInfo(DWORD* bindf, BINDINFO* bind_info) {
  DLOG(INFO) << __FUNCTION__ << me() << base::StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  HRESULT hr = S_OK;
  if (delegate_)
    delegate_->GetBindInfo(bindf, bind_info);
  return hr;
}

HRESULT BSCBImpl::OnDataAvailable(DWORD bscf, DWORD size,
                                   FORMATETC* format_etc, STGMEDIUM* stgmed) {
  DLOG(INFO) << __FUNCTION__ << me() << base::StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  HRESULT hr = S_OK;
  if (delegate_)
    hr = delegate_->OnDataAvailable(bscf, size, format_etc, stgmed);
  return hr;
}

HRESULT BSCBImpl::OnObjectAvailable(REFIID iid, IUnknown* unk) {
  DLOG(INFO) << __FUNCTION__ << me() << base::StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  HRESULT hr = S_OK;
  if (delegate_)
    delegate_->OnObjectAvailable(iid, unk);
  return hr;
}

// IBindStatusCallbackEx
HRESULT BSCBImpl::GetBindInfoEx(DWORD* bindf, BINDINFO* bind_info,
                                DWORD* bindf2, DWORD* reserved) {
  DLOG(INFO) << __FUNCTION__ << me() << base::StringPrintf(" tid=%i",
      PlatformThread::CurrentId());
  HRESULT hr = S_OK;
  if (delegate_) {
    ScopedComPtr<IBindStatusCallbackEx> bscbex;
    bscbex.QueryFrom(delegate_);
    if (bscbex)
      hr = bscbex->GetBindInfoEx(bindf, bind_info, bindf2, reserved);
  }
  return hr;
}

HRESULT BSCBImpl::BeginningTransaction(LPCWSTR url, LPCWSTR headers,
                                       DWORD reserved,
                                       LPWSTR* additional_headers) {
  DLOG(INFO) << __FUNCTION__ << me() << base::StringPrintf(" tid=%i",
      PlatformThread::CurrentId());

  HRESULT hr = S_OK;
  if (delegate_) {
    ScopedComPtr<IHttpNegotiate> http_negotiate;
    http_negotiate.QueryFrom(delegate_);
    if (http_negotiate) {
      hr = http_negotiate->BeginningTransaction(url, headers, reserved,
                                                additional_headers);
    }
  }

  DLOG_IF(ERROR, FAILED(hr)) << __FUNCTION__;
  return hr;
}

HRESULT BSCBImpl::OnResponse(DWORD response_code, LPCWSTR response_headers,
                             LPCWSTR request_headers,
                             LPWSTR* additional_headers) {
  DLOG(INFO) << __FUNCTION__ << me() << base::StringPrintf(" tid=%i",
      PlatformThread::CurrentId());

  HRESULT hr = S_OK;
  if (delegate_) {
    ScopedComPtr<IHttpNegotiate> http_negotiate;
    http_negotiate.QueryFrom(delegate_);
    if (http_negotiate) {
      hr = http_negotiate->OnResponse(response_code, response_headers,
                                      request_headers, additional_headers);
    }
  }
  return hr;
}

HRESULT BSCBImpl::GetRootSecurityId(BYTE* security_id, DWORD* security_id_size,
                                    DWORD_PTR reserved) {
  HRESULT hr = S_OK;
  if (delegate_) {
    ScopedComPtr<IHttpNegotiate2> http_negotiate;
    http_negotiate.QueryFrom(delegate_);
    if (http_negotiate) {
      hr = http_negotiate->GetRootSecurityId(security_id, security_id_size,
                                             reserved);
    }
  }
  return hr;
}

HRESULT BSCBImpl::GetSerializedClientCertContext(BYTE** cert,
                                                 DWORD* cert_size) {
  HRESULT hr = S_OK;
  if (delegate_) {
    ScopedComPtr<IHttpNegotiate3> http_negotiate;
    http_negotiate.QueryFrom(delegate_);
    if (http_negotiate) {
      return http_negotiate->GetSerializedClientCertContext(cert, cert_size);
    }
  }
  return hr;
}

