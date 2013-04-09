// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/common/win/cloud_print_utils.h"

#include <windows.h>

namespace cloud_print {

HRESULT GetLastHResult() {
  DWORD error_code = GetLastError();
  return error_code ? HRESULT_FROM_WIN32(error_code) : E_FAIL;
}

string16 LoadLocalString(DWORD id) {
  static wchar_t dummy = L'\0';
  HMODULE module = NULL;
  ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT |
                      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, &dummy, &module);
  LPCWSTR buffer = NULL;
  // If the last parameter is 0, LoadString assume that 3rd parameter type is
  // LPCWSTR* and assign pointer to read-only memory with resource.
  int count = ::LoadString(module, id, reinterpret_cast<LPWSTR>(&buffer), 0);
  if (!buffer)
    return string16();
  return string16(buffer, buffer + count);
}

string16 GetErrorMessage(HRESULT hr) {
  LPWSTR buffer = NULL;
  ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
                  FORMAT_MESSAGE_ALLOCATE_BUFFER,
                  0, hr, 0, reinterpret_cast<LPWSTR>(&buffer), 0, NULL);
  string16 result(buffer);
  ::LocalFree(buffer);
  return result;
}

}  // namespace cloud_print
