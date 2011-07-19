// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_PROTOCOL_H_
#define CHROME_FRAME_CHROME_PROTOCOL_H_

#include <atlbase.h>
#include <atlcom.h>
#include "chrome_frame/resource.h"
#include "grit/chrome_frame_resources.h"

// Include without path to make GYP build see it.
#include "chrome_tab.h"  // NOLINT

// ChromeProtocol
class ATL_NO_VTABLE ChromeProtocol
    : public CComObjectRootEx<CComSingleThreadModel>,
      public CComCoClass<ChromeProtocol, &CLSID_ChromeProtocol>,
      public IInternetProtocol {
 public:
  ChromeProtocol() {
  }
  DECLARE_REGISTRY_RESOURCEID(IDR_CHROMEPROTOCOL)

  BEGIN_COM_MAP(ChromeProtocol)
    COM_INTERFACE_ENTRY(IInternetProtocol)
    COM_INTERFACE_ENTRY(IInternetProtocolRoot)
  END_COM_MAP()

  DECLARE_PROTECT_FINAL_CONSTRUCT()

  HRESULT FinalConstruct() {
    return S_OK;
  }
  void FinalRelease() {
  }

 public:
  // IInternetProtocolRoot
  STDMETHOD(Start)(LPCWSTR url,
                   IInternetProtocolSink* prot_sink,
                   IInternetBindInfo* bind_info,
                   DWORD flags,
                   DWORD reserved);
  STDMETHOD(Continue)(PROTOCOLDATA* protocol_data);
  STDMETHOD(Abort)(HRESULT reason, DWORD options);
  STDMETHOD(Terminate)(DWORD options);
  STDMETHOD(Suspend)();
  STDMETHOD(Resume)();

  // IInternetProtocol based on IInternetProtocolRoot
  STDMETHOD(Read)(void* buffer,
                  ULONG buffer_size_in_bytes,
                  ULONG* bytes_read);
  STDMETHOD(Seek)(LARGE_INTEGER move_by,
                  DWORD origin,
                  ULARGE_INTEGER* new_position);
  STDMETHOD(LockRequest)(DWORD options);
  STDMETHOD(UnlockRequest)(void);
};

#endif  // CHROME_FRAME_CHROME_PROTOCOL_H_
