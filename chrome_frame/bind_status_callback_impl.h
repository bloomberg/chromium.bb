// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_BIND_STATUS_CALLBACK_IMPL_H_
#define CHROME_FRAME_BIND_STATUS_CALLBACK_IMPL_H_

#include <atlbase.h>
#include <atlcom.h>
#include <string>
#include <urlmon.h>

#include "base/win/scoped_comptr.h"
#include "chrome_frame/utils.h"

// A generic base class for IBindStatus callback implementation.
// If initialized with delegate, it will hand over all the calls
// to the delegate. This can also be used as a base class to
// provide the base implementation by not providing any delegate.
class BSCBImpl
  : public CComObjectRootEx<CComMultiThreadModel>,
    public IBindStatusCallbackEx,
    public IHttpNegotiate3,
    public IServiceProvider {
 public:
  BSCBImpl();
  ~BSCBImpl();

BEGIN_COM_MAP(BSCBImpl)
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

  void Initialize(IBindStatusCallback* original);
  HRESULT AttachToBind(IBindCtx* original);
  HRESULT ReleaseBind();

  // For the COM_INTERFACE_ENTRY_IF_DELEGATE_SUPPORTS macro.
  IBindStatusCallback* delegate() const {
    return delegate_;
  }

  IBindCtx* bind_ctx() const {
    return bind_ctx_;
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
  // used for logging.
  std::string me();

  base::win::ScopedComPtr<IBindStatusCallback> delegate_;
  base::win::ScopedComPtr<IBindCtx> bind_ctx_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BSCBImpl);
};

#endif  // CHROME_FRAME_BIND_STATUS_CALLBACK_IMPL_H_
