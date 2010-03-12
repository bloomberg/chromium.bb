// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_URLMON_BIND_STATUS_CALLBACK_H_
#define CHROME_FRAME_URLMON_BIND_STATUS_CALLBACK_H_

#include <atlbase.h>
#include <atlcom.h>

#include "chrome_frame/urlmon_moniker.h"

// Our implementation of IBindStatusCallback that allows us to sit on the
// sidelines and cache all the data that passes by (using the RequestData
// class).  That cache can then be used by other monikers for the same URL
// that immediately follow.
class CFUrlmonBindStatusCallback
  : public CComObjectRootEx<CComMultiThreadModel>,
    public IBindStatusCallbackEx,
    public IHttpNegotiate3,
    public IServiceProvider {
 public:
  CFUrlmonBindStatusCallback();
  ~CFUrlmonBindStatusCallback();

  // used for logging.
  std::string me();

BEGIN_COM_MAP(CFUrlmonBindStatusCallback)
  COM_INTERFACE_ENTRY(IBindStatusCallback)
  COM_INTERFACE_ENTRY(IHttpNegotiate)
  COM_INTERFACE_ENTRY_IF_DELEGATE_SUPPORTS(IBindStatusCallbackEx)
  COM_INTERFACE_ENTRY_IF_DELEGATE_SUPPORTS(IHttpNegotiate2)
  COM_INTERFACE_ENTRY_IF_DELEGATE_SUPPORTS(IHttpNegotiate3)
  COM_INTERFACE_ENTRY_IF_DELEGATE_SUPPORTS(IServiceProvider)
  COM_INTERFACE_ENTRY_FUNC_BLIND(0, DelegateQI)
END_COM_MAP()

  static STDMETHODIMP DelegateQI(void* obj, REFIID iid, void** ret,
                                 DWORD cookie);

  HRESULT Initialize(IBindCtx* bind_ctx, RequestHeaders* headers);

  // For the COM_INTERFACE_ENTRY_IF_DELEGATE_SUPPORTS macro.
  IBindStatusCallback* delegate() const {
    return delegate_;
  }

  RequestData* request_data() {
    return data_;
  }

  // IServiceProvider
  STDMETHOD(QueryService)(REFGUID service, REFIID iid, void** object);

  // IBindStatusCallback
  STDMETHOD(OnStartBinding)(DWORD reserved, IBinding* binding);
  STDMETHOD(GetPriority)(LONG* priority);
  STDMETHOD(OnLowResource)(DWORD reserved);
  STDMETHOD(OnProgress)(ULONG progress, ULONG progress_max, ULONG status_code,
                        LPCWSTR status_text);
  STDMETHOD(OnStopBinding)(HRESULT hresult, LPCWSTR error);
  STDMETHOD(GetBindInfo)(DWORD* bindf, BINDINFO* bind_info);
  STDMETHOD(OnDataAvailable)(DWORD bscf, DWORD size, FORMATETC* format_etc,
                             STGMEDIUM* stgmed);
  STDMETHOD(OnObjectAvailable)(REFIID iid, IUnknown* unk);

  // IBindStatusCallbackEx
  STDMETHOD(GetBindInfoEx)(DWORD* bindf, BINDINFO* bind_info, DWORD* bindf2,
                           DWORD* reserved);

  // IHttpNegotiate
  STDMETHOD(BeginningTransaction)(LPCWSTR url, LPCWSTR headers, DWORD reserved,
                                  LPWSTR* additional_headers);
  STDMETHOD(OnResponse)(DWORD response_code, LPCWSTR response_headers,
                        LPCWSTR request_headers, LPWSTR* additional_headers);

  // IHttpNegotiate2
  STDMETHOD(GetRootSecurityId)(BYTE* security_id, DWORD* security_id_size,
                               DWORD_PTR reserved);

  // IHttpNegotiate3
  STDMETHOD(GetSerializedClientCertContext)(BYTE** cert, DWORD* cert_size);

 protected:
  ScopedComPtr<IBindStatusCallback> delegate_;
  ScopedComPtr<IBindCtx> bind_ctx_;
  ScopedComPtr<SimpleBindingImpl, &GUID_NULL> binding_delegate_;
  bool only_buffer_;
  scoped_refptr<RequestData> data_;
  std::wstring redirected_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CFUrlmonBindStatusCallback);
};

#endif  // CHROME_FRAME_URLMON_BIND_STATUS_CALLBACK_H_

