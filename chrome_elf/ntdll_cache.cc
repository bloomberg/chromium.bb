// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <windows.h>

#include "base/win/pe_image.h"
#include "chrome_elf/ntdll_cache.h"

FunctionLookupTable g_ntdll_lookup;

namespace {

bool EnumExportsCallback(const base::win::PEImage& image,
                         DWORD ordinal,
                         DWORD hint,
                         LPCSTR name,
                         PVOID function_addr,
                         LPCSTR forward,
                         PVOID cookie) {
  // Our lookup only cares about named functions that are in ntdll, so skip
  // unnamed or forwarded exports.
  if (name && function_addr)
    g_ntdll_lookup[std::string(name)] = function_addr;

  return true;
}

}  // namespace

void InitCache() {
  HMODULE ntdll_handle = ::GetModuleHandle(L"ntdll.dll");

  base::win::PEImage ntdll_image(ntdll_handle);

  ntdll_image.EnumExports(EnumExportsCallback, NULL);
}
