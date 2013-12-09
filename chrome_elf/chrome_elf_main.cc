// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome_elf/chrome_elf_main.h"

#include "chrome_elf/ntdll_cache.h"

void InitChromeElf() {
  // This method is a no-op which may be called to force a load-time dependency
  // on chrome_elf.dll.
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH)
    InitCache();
  return TRUE;
}
