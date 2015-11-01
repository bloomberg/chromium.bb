// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some pointless code that will become a DLL with some exports and relocs.

#include <windows.h>

namespace {

void (*g_somestate)(int) = nullptr;
int g_somevalue = 0;

}  // namespace

extern "C"
void DummyExport(int foo) {
  g_somevalue = foo;
}

extern "C"
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH)
    g_somestate = &DummyExport;
  else if (reason == DLL_PROCESS_DETACH)
    g_somestate = nullptr;

  return TRUE;
}
