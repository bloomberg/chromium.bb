// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for testing.

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "ceee/testing/utils/test_utils.h"

namespace testing {

HRESULT GetConnectionCount(IUnknown* container,
                           REFIID connection_point_id,
                           size_t* num_connections) {
  if (!num_connections)
    return E_POINTER;

  *num_connections = 0;

  CComPtr<IConnectionPointContainer> cpc;
  HRESULT hr = container->QueryInterface(&cpc);
  if (FAILED(hr))
    return hr;

  CHECK(cpc != NULL);

  CComPtr<IConnectionPoint> cp;
  hr = cpc->FindConnectionPoint(connection_point_id, &cp);

  CComPtr<IEnumConnections> enum_connections;
  if (SUCCEEDED(hr))
    hr = cp->EnumConnections(&enum_connections);

  if (FAILED(hr))
    return hr;

  while (true) {
    CONNECTDATA conn = {};
    ULONG fetched = 0;
    hr = enum_connections->Next(1, &conn, &fetched);
    if (FAILED(hr)) {
      CHECK(conn.pUnk == NULL);
      return hr;
    }
    if (hr == S_FALSE) {
      CHECK(conn.pUnk == NULL);
      return S_OK;
    }
    CHECK(NULL != conn.pUnk);
    conn.pUnk->Release();

    (*num_connections)++;
  }
  NOTREACHED();
  return E_UNEXPECTED;
}

bool LogDisabler::DropMessageHandler(int severity, const char* file, int line,
    size_t message_start, const std::string& str) {
  // Message is handled, no further processing.
  return true;
}

LogDisabler::LogDisabler() {
  old_handler_ = logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(DropMessageHandler);
}

LogDisabler::~LogDisabler() {
  logging::SetLogMessageHandler(old_handler_);
}

PathServiceOverrider::PathServiceOverrider(int key, const FilePath& path) {
  key_ = key;
  PathService::Get(key, &original_path_);
  PathService::Override(key, path);
}

PathServiceOverrider::~PathServiceOverrider() {
  PathService::Override(key_, original_path_);
}


namespace internal {

void WriteVariant(const VARIANT& var, ::std::ostream* os) {
  *os << "{type=";

  static struct {
      VARTYPE vt;
      const char *name;
  } vartypes[] = {
#define TYP(x) { x, #x },
    TYP(VT_EMPTY)
    TYP(VT_NULL)
    TYP(VT_I2)
    TYP(VT_I4)
    TYP(VT_R4)
    TYP(VT_R8)
    TYP(VT_CY)
    TYP(VT_DATE)
    TYP(VT_BSTR)
    TYP(VT_DISPATCH)
    TYP(VT_ERROR)
    TYP(VT_BOOL)
    TYP(VT_VARIANT)
    TYP(VT_UNKNOWN)
    TYP(VT_DECIMAL)
    TYP(VT_I1)
    TYP(VT_UI1)
    TYP(VT_UI2)
    TYP(VT_UI4)
    TYP(VT_I8)
    TYP(VT_UI8)
    TYP(VT_INT)
    TYP(VT_UINT)
    TYP(VT_VOID)
    TYP(VT_HRESULT)
    TYP(VT_PTR)
    TYP(VT_SAFEARRAY)
    TYP(VT_CARRAY)
    TYP(VT_USERDEFINED)
    TYP(VT_LPSTR)
    TYP(VT_LPWSTR)
    TYP(VT_RECORD)
    TYP(VT_INT_PTR)
    TYP(VT_UINT_PTR)
    TYP(VT_FILETIME)
    TYP(VT_BLOB)
    TYP(VT_STREAM)
    TYP(VT_STORAGE)
    TYP(VT_STREAMED_OBJECT)
    TYP(VT_STORED_OBJECT)
    TYP(VT_BLOB_OBJECT)
    TYP(VT_CF)
    TYP(VT_CLSID)
    TYP(VT_VERSIONED_STREAM)
    TYP(VT_BSTR_BLOB)
    TYP(VT_ILLEGAL)
#undef TYP
  };

  bool found = false;
  for (int i = 0; !found && i < ARRAYSIZE(vartypes); ++i) {
    if (vartypes[i].vt == (var.vt & VT_TYPEMASK)) {
      *os << vartypes[i].name;
      found = true;
    }
  }

  if (!found)
    *os << "*ILLEGAL*";

#define ATTRBIT(x) if (var.vt & x) *os << "| " #x;
  ATTRBIT(VT_VECTOR)
  ATTRBIT(VT_ARRAY)
  ATTRBIT(VT_BYREF)
  ATTRBIT(VT_RESERVED)
#undef ATTRBIT

  CComVariant copy;
  if (SUCCEEDED(copy.ChangeType(VT_BSTR, &var))) {
    *os << ", value=\""
        << WideToUTF8(std::wstring(copy.bstrVal, ::SysStringLen(copy.bstrVal)))
        << "\"";
  }

  *os << "}";
}

}  // namespace testing::internal

}  // namespace testing
