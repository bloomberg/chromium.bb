// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_verification/win/imported_module_verification.h"

#include <Windows.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>
#include "base/logging.h"
#include "base/strings/string16.h"
#include "chrome/browser/install_verification/win/module_info.h"

namespace {

// We must make sure not to include modules here that are likely to get unloaded
// because the scanning of the module is not done within a loader lock, so is
// not resilient to changes made to the modules list.
const wchar_t* const kModulesToScan[] = {
  L"chrome.dll",
  L"kernel32.dll",
  L"user32.dll"
};

bool AddressBeyondRange(const ModuleInfo& module, uintptr_t address) {
  return module.base_address + module.size < address;
}

void ScanImportAddressTable(
    HMODULE module_handle,
    const std::set<ModuleInfo>& loaded_modules,
    std::set<base::string16>* imported_modules) {
  DCHECK(module_handle);
  DCHECK(imported_modules);

  // To find the Import Address Table address, we start from the DOS header.
  // The module handle is actually the base address where the header is.
  IMAGE_DOS_HEADER* dos_header =
      reinterpret_cast<IMAGE_DOS_HEADER*>(module_handle);
  // The e_lfanew is an offset from the DOS header to the NT header. It should
  // never be 0.
  DCHECK(dos_header->e_lfanew);
  IMAGE_NT_HEADERS* nt_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(
      module_handle + dos_header->e_lfanew / sizeof(uintptr_t));
  // For modules that have an import address table, its offset from the
  // DOS header is stored in the second data directory's VirtualAddress.
  if (!nt_headers->OptionalHeader.DataDirectory[1].VirtualAddress)
    return;
  IMAGE_IMPORT_DESCRIPTOR* image_descriptor =
      reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(module_handle +
          nt_headers->OptionalHeader.DataDirectory[1].VirtualAddress /
              sizeof(uintptr_t));
  // The list of Import Address Tables ends with an empty {0} descriptor.
  while (image_descriptor->Characteristics) {
    uintptr_t* address = reinterpret_cast<uintptr_t*>(
        module_handle + image_descriptor->FirstThunk / sizeof(uintptr_t));
    // Each section of the Import Address Table ends with a NULL funcpointer.
    while (*address) {
      std::set<ModuleInfo>::const_iterator lower_bound = std::lower_bound(
          loaded_modules.begin(), loaded_modules.end(), *address,
          AddressBeyondRange);
      if (lower_bound != loaded_modules.end() &&
          lower_bound->ContainsAddress(*address)) {
        imported_modules->insert(lower_bound->name);
      }
      ++address;
    }
    image_descriptor += sizeof(image_descriptor) / sizeof(uintptr_t);
  }
}

}  // namespace

void VerifyImportedModules(const std::set<ModuleInfo>& loaded_modules,
                           const ModuleIDs& module_ids,
                           ModuleVerificationDelegate* delegate){
// Note that module verification is temporarily disabled for 64-bit builds.
// See crbug.com/316274.
#if !defined(_WIN64)
  std::set<base::string16> imported_module_names;
  for (size_t i = 0; i < arraysize(kModulesToScan); ++i) {
    HMODULE module_handle = ::GetModuleHandle(kModulesToScan[i]);
    if (module_handle) {
      ScanImportAddressTable(module_handle,
                             loaded_modules,
                             &imported_module_names);
    }
  }

  std::vector<std::string> module_name_digests;
  std::transform(imported_module_names.begin(),
                 imported_module_names.end(),
                 std::back_inserter(module_name_digests),
                 &CalculateModuleNameDigest);
  ReportModuleMatches(module_name_digests, module_ids, delegate);
#endif
}
