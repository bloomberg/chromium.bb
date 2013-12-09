// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <windows.h>

#include "chrome_elf/ntdll_cache.h"

FunctionLookupTable g_ntdll_lookup;

void InitCache() {
  HMODULE ntdll_handle = ::GetModuleHandle(L"ntdll.dll");

  // To find the Export Address Table address, we start from the DOS header.
  // The module handle is actually the address of the header.
  IMAGE_DOS_HEADER* dos_header =
      reinterpret_cast<IMAGE_DOS_HEADER*>(ntdll_handle);
  // The e_lfanew is an offset from the DOS header to the NT header. It should
  // never be 0.
  IMAGE_NT_HEADERS* nt_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(
      ntdll_handle + dos_header->e_lfanew / sizeof(uint32_t));
  // For modules that have an import address table, its offset from the
  // DOS header is stored in the second data directory's VirtualAddress.
  if (!nt_headers->OptionalHeader.DataDirectory[0].VirtualAddress)
    return;

  BYTE* base_addr = reinterpret_cast<BYTE*>(ntdll_handle);

  IMAGE_DATA_DIRECTORY* exports_data_dir =
      &nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

  IMAGE_EXPORT_DIRECTORY* exports = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
      base_addr + exports_data_dir->VirtualAddress);

  WORD* ordinals = reinterpret_cast<WORD*>(
      base_addr + exports->AddressOfNameOrdinals);
  DWORD* names = reinterpret_cast<DWORD*>(
      base_addr + exports->AddressOfNames);
  DWORD* funcs = reinterpret_cast<DWORD*>(
      base_addr + exports->AddressOfFunctions);
  int num_entries = exports->NumberOfNames;

  for (int i = 0; i < num_entries; i++) {
    char* name = reinterpret_cast<char*>(base_addr + names[i]);
    WORD ord =  ordinals[i];
    DWORD func = funcs[ord];
    FARPROC func_addr = reinterpret_cast<FARPROC>(func + base_addr);
    g_ntdll_lookup[std::string(name)] = func_addr;
  }
}
