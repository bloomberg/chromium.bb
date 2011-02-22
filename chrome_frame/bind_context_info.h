// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_BIND_CONTEXT_INFO_H_
#define CHROME_FRAME_BIND_CONTEXT_INFO_H_

#include <atlbase.h>
#include <atlcom.h>

#include "base/win/scoped_comptr.h"
#include "chrome_frame/protocol_sink_wrap.h"

class __declspec(uuid("71CC3EC7-7E8A-457f-93BC-1090CF31CC18"))
IBindContextInfoInternal : public IUnknown {
 public:
  STDMETHOD(GetCppObject)(void** me) = 0;
};

// This class maintains contextual information used by ChromeFrame.
// This information is maintained in the bind context.
// Association with GUID_NULL is for convenience.
class __declspec(uuid("00000000-0000-0000-0000-000000000000")) BindContextInfo
  : public CComObjectRootEx<CComMultiThreadModel>,
    public IBindContextInfoInternal {
 public:
  BindContextInfo();
  ~BindContextInfo();

  BEGIN_COM_MAP(BindContextInfo)
    COM_INTERFACE_ENTRY(IBindContextInfoInternal)
    COM_INTERFACE_ENTRY_AGGREGATE(IID_IMarshal, ftm_)
  END_COM_MAP()

  // Returns the BindContextInfo instance associated with the bind
  // context. Creates it if needed.
  // The returned info object will be AddRef-ed on return, so use
  // ScopedComPtr<>::Receive() to receive this pointer.
  static HRESULT FromBindContext(IBindCtx* bind_context,
                                 BindContextInfo** info);

  void set_chrome_request(bool chrome_request) {
    chrome_request_ = chrome_request;
  }

  bool chrome_request() const {
    return chrome_request_;
  }

  void set_no_cache(bool no_cache) {
    no_cache_ = no_cache;
  }

  bool no_cache() const {
    return no_cache_;
  }

  bool is_switching() const {
    return is_switching_;
  }

  void SetToSwitch(IStream* cache);

  IStream* cache() {
    return cache_;
  }

  void set_prot_data(ProtData* data) {
    prot_data_ = data;
  }

  scoped_refptr<ProtData> get_prot_data() {
    return prot_data_;
  }

  bool has_prot_data() const {
    return prot_data_.get() != NULL;
  }

  void set_protocol(IInternetProtocol* protocol) {
    protocol_ = protocol;
  }

  IInternetProtocol* protocol() {
    return protocol_.get();
  }

  // Returns the url being navigated to. We retrieve the url from the ProtData
  // instance which wraps the underlying protocol sink.
  std::wstring GetUrl();

 protected:
  STDMETHOD(GetCppObject)(void** me) {
    DCHECK(me);
    AddRef();
    *me = static_cast<BindContextInfo*>(this);
    return S_OK;
  }

  HRESULT Initialize(IBindCtx* bind_ctx);

 private:
  base::win::ScopedComPtr<IStream> cache_;
  bool no_cache_;
  bool chrome_request_;
  bool is_switching_;
  base::win::ScopedComPtr<IUnknown> ftm_;
  scoped_refptr<ProtData> prot_data_;
  ScopedComPtr<IInternetProtocol> protocol_;

  DISALLOW_COPY_AND_ASSIGN(BindContextInfo);
};

#endif  // CHROME_FRAME_BIND_CONTEXT_INFO_H_
