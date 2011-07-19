// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome_frame/crash_reporting/nt_loader.h"

#include <winnt.h>
#include <winternl.h>
#include "base/logging.h"

namespace nt_loader {

LDR_DATA_TABLE_ENTRY* GetLoaderEntry(HMODULE module) {
  // Make sure we own the loader's lock on entry here.
  DCHECK(OwnsCriticalSection(GetLoaderLock()));
  PEB* peb = GetCurrentPeb();

  LIST_ENTRY* head = &peb->Ldr->InLoadOrderModuleList;
  for (LIST_ENTRY* entry = head->Flink; entry != head; entry = entry->Flink) {
    LDR_DATA_TABLE_ENTRY* ldr_entry =
        CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

    if (reinterpret_cast<HMODULE>(ldr_entry->DllBase) == module)
      return ldr_entry;
  }

  return NULL;
}

}  // namespace nt_loader
