// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_URLMON_MONIKER_TESTS_H_
#define CHROME_FRAME_TEST_URLMON_MONIKER_TESTS_H_

#include <atlbase.h>
#include <atlcom.h>

#include "chrome_frame/bho.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

class MockBindingImpl
  : public SimpleBindingImpl,
    public IServiceProvider {
 public:
BEGIN_COM_MAP(MockBindingImpl)
  COM_INTERFACE_ENTRY(IBinding)
  COM_INTERFACE_ENTRY(IServiceProvider)
END_COM_MAP()

  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, Abort,
                             HRESULT ());  // NOLINT
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, QueryService,
                             HRESULT (REFGUID svc,  // NOLINT
                                      REFIID riid,
                                      void** obj));
  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, GetBindResult,
                             HRESULT (CLSID* protocol,  // NOLINT
                                      DWORD* result_code,
                                      LPOLESTR* result,
                                      DWORD* reserved));
};

class MockHttpNegotiateImpl
  : public CComObjectRootEx<CComSingleThreadModel>,
    public IHttpNegotiate {
 public:
BEGIN_COM_MAP(MockHttpNegotiateImpl)
  COM_INTERFACE_ENTRY(IHttpNegotiate)
END_COM_MAP()
  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, BeginningTransaction,
                             HRESULT (LPCWSTR url,  // NOLINT
                                      LPCWSTR headers,
                                      DWORD reserved,
                                      LPWSTR* additional));
  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, OnResponse,
                             HRESULT (DWORD code,  // NOLINT
                                      LPCWSTR response_headers,
                                      LPCWSTR request_headers,
                                      LPWSTR* additional));
};

class MockBindStatusCallbackImpl
  : public CComObjectRootEx<CComSingleThreadModel>,
    public IBindStatusCallback {
 public:
BEGIN_COM_MAP(MockBindStatusCallbackImpl)
  COM_INTERFACE_ENTRY(IBindStatusCallback)
END_COM_MAP()
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, OnStartBinding,
      HRESULT (DWORD reserved, IBinding* binding));  // NOLINT

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetPriority,
      HRESULT (LONG* priority));  // NOLINT

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, OnLowResource,
      HRESULT (DWORD reserved));   // NOLINT

  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, OnProgress,
      HRESULT (ULONG progress,  // NOLINT
               ULONG max,
               ULONG status,
               LPCWSTR text));

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, OnStopBinding,
      HRESULT (HRESULT hr,  // NOLINT
               LPCWSTR error));

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, GetBindInfo,
      HRESULT (DWORD* flags,  // NOLINT
               BINDINFO* bind_info));

  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, OnDataAvailable,
      HRESULT (DWORD flags,  // NOLINT
               DWORD size,
               FORMATETC* format,
               STGMEDIUM* storage));

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, OnObjectAvailable,
      HRESULT (REFIID riid,  // NOLINT
               IUnknown* unk));

  static void ReadAllData(STGMEDIUM* storage) {
    DCHECK(storage);
    DCHECK(storage->tymed == TYMED_ISTREAM);
    char buffer[0xff];
    HRESULT hr;
    DWORD read = 0;
    do {
      hr = storage->pstm->Read(buffer, sizeof(buffer), &read);
    } while (hr == S_OK && read);
  }

  static void SetSyncBindInfo(DWORD* flags, BINDINFO* bind_info) {
    DCHECK(flags);
    DCHECK(bind_info);
    *flags = BINDF_PULLDATA | BINDF_NOWRITECACHE;
    DCHECK(bind_info->cbSize >= sizeof(BINDINFO));
    bind_info->dwBindVerb = BINDVERB_GET;
    memset(&bind_info->stgmedData, 0, sizeof(STGMEDIUM));
    bind_info->grfBindInfoF = 0;
    bind_info->szCustomVerb = NULL;
  }

  static void SetAsyncBindInfo(DWORD* flags, BINDINFO* bind_info) {
    DCHECK(flags);
    DCHECK(bind_info);
    *flags = BINDF_ASYNCHRONOUS | BINDF_ASYNCSTORAGE | BINDF_PULLDATA |
             BINDF_NOWRITECACHE;
    DCHECK(bind_info->cbSize >= sizeof(BINDINFO));
    bind_info->dwBindVerb = BINDVERB_GET;
    memset(&bind_info->stgmedData, 0, sizeof(STGMEDIUM));
    bind_info->grfBindInfoF = 0;
    bind_info->szCustomVerb = NULL;
  }
};

class TestNavigationManager : public NavigationManager {
 public:
  TestNavigationManager() {
  }

  bool HasRequestData() const {
    return request_data_.get() != NULL;
  }
};

#endif  // CHROME_FRAME_TEST_URLMON_MONIKER_TESTS_H_

