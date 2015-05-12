// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome_elf/blacklist/blacklist.h"

extern "C" void InitBlacklistTestDll() {}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    blacklist::Initialize(true);  // force always on, no beacon
  }

  return TRUE;
}
