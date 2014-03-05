// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/ntdll_cache.h"

#include <stdint.h>
#include <windows.h>

#include "base/basictypes.h"
#include "chrome_elf/thunk_getter.h"
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/service_resolver.h"

// Allocate storage for thunks in a page of this module to save on doing
// an extra allocation at run time.
#pragma section(".crthunk", read, execute)
__declspec(allocate(".crthunk")) sandbox::ThunkData g_nt_thunk_storage;

FunctionLookupTable g_ntdll_lookup;

void InitCache() {
  HMODULE ntdll_handle = ::GetModuleHandle(sandbox::kNtdllName);

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

  const bool kRelaxed = true;

  // Create a thunk via the appropriate ServiceResolver instance.
  sandbox::ServiceResolverThunk* thunk = GetThunk(kRelaxed);

  if (thunk) {
    BYTE* thunk_storage = reinterpret_cast<BYTE*>(&g_nt_thunk_storage);

    // Mark the thunk storage as readable and writeable, since we
    // ready to write to it.
    DWORD old_protect = 0;
    if (!::VirtualProtect(&g_nt_thunk_storage,
                          sizeof(g_nt_thunk_storage),
                          PAGE_EXECUTE_READWRITE,
                          &old_protect)) {
      return;
    }

    size_t storage_used = 0;
    NTSTATUS ret = thunk->CopyThunk(::GetModuleHandle(sandbox::kNtdllName),
                                    "NtCreateFile",
                                    thunk_storage,
                                    sizeof(sandbox::ThunkData),
                                    &storage_used);
    delete thunk;

    // Ensure that the pointer to the old function can't be changed.
    ::VirtualProtect(&g_nt_thunk_storage,
                     sizeof(g_nt_thunk_storage),
                     PAGE_EXECUTE_READ,
                     &old_protect);

    if (NT_SUCCESS(ret)) {
      // Add an entry in the lookup table for the thunk.
      g_ntdll_lookup["NtCreateFile"] =
          reinterpret_cast<FARPROC>(&g_nt_thunk_storage);
    }
  }
}
