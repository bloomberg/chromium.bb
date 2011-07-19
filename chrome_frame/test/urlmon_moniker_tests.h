// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_URLMON_MONIKER_TESTS_H_
#define CHROME_FRAME_TEST_URLMON_MONIKER_TESTS_H_

#include <atlbase.h>
#include <atlcom.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

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
};

class MockBindCtxImpl
  : public CComObjectRootEx<CComSingleThreadModel>,
    public IBindCtx {
 public:
BEGIN_COM_MAP(MockBindCtxImpl)
  COM_INTERFACE_ENTRY(IBindCtx)
END_COM_MAP()

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, RegisterObjectBound,
      HRESULT (IUnknown* object));  // NOLINT

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, RevokeObjectBound,
      HRESULT (IUnknown* object));  // NOLINT

  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, ReleaseBoundObjects,
      HRESULT ());  // NOLINT

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, SetBindOptions,
      HRESULT (BIND_OPTS* options));   // NOLINT

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetBindOptions,
      HRESULT (BIND_OPTS* options));   // NOLINT

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, GetRunningObjectTable,
      HRESULT (IRunningObjectTable** rot));  // NOLINT

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, RegisterObjectParam,
      HRESULT (LPOLESTR key,  // NOLINT
               IUnknown* param));

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, GetObjectParam,
      HRESULT (LPOLESTR key,  // NOLINT
               IUnknown** param));

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, EnumObjectParam,
      HRESULT (IEnumString** enum_params));  // NOLINT

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, RevokeObjectParam,
      HRESULT (LPOLESTR key));  // NOLINT
};

class MockMonikerImpl
  : public CComObjectRootEx<CComSingleThreadModel>,
    public IMoniker {
 public:
BEGIN_COM_MAP(MockMonikerImpl)
  COM_INTERFACE_ENTRY(IMoniker)
END_COM_MAP()

  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, BindToObject,
      HRESULT (IBindCtx* bind_context,  // NOLINT
               IMoniker* left,
               REFIID result_iid,
               void** object));

  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, BindToStorage,
      HRESULT (IBindCtx* bind_context,  // NOLINT
               IMoniker* left,
               REFIID result_iid,
               void** storage));

  MOCK_METHOD4_WITH_CALLTYPE(__stdcall, Reduce,
      HRESULT (IBindCtx* bind_context,  // NOLINT
               DWORD reduce_depth,
               IMoniker* left,
               IMoniker** reduced));

  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, ComposeWith,
      HRESULT (IBindCtx* right,  // NOLINT
               BOOL is_not_generic,
               IMoniker** composite));

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, Enum,
      HRESULT (BOOL is_forward,  // NOLINT
               IEnumMoniker** moniker_enum));

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, IsEqual,
      HRESULT (IMoniker* other));  // NOLINT

  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, IsRunning,
      HRESULT (IBindCtx* bind_context,  // NOLINT
               IMoniker* left,
               IMoniker** newly_running));

  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, GetTimeOfLastChange,
      HRESULT (IBindCtx* bind_context,  // NOLINT
               IMoniker* left,
                FILETIME *pFileTime));

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, Inverse,
      HRESULT (IMoniker** inversed));   // NOLINT

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, CommonPrefixWith,
      HRESULT (IMoniker* other,  // NOLINT
               IMoniker** prefix));

  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, RelativePathTo,
      HRESULT (IMoniker* other,  // NOLINT
               IMoniker** relative));

    MOCK_METHOD3_WITH_CALLTYPE(__stdcall, GetDisplayName,
      HRESULT (IBindCtx* bind_context,  // NOLINT
               IMoniker* left,
               LPOLESTR* display_name));

    MOCK_METHOD5_WITH_CALLTYPE(__stdcall, ParseDisplayName,
      HRESULT (IBindCtx* bind_context,  // NOLINT
               IMoniker* left,
               LPOLESTR display_name,
               ULONG *pchEaten,
               IMoniker** ret));

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, IsSystemMoniker,
      HRESULT (DWORD* is_system));  // NOLINT
};

#endif  // CHROME_FRAME_TEST_URLMON_MONIKER_TESTS_H_
