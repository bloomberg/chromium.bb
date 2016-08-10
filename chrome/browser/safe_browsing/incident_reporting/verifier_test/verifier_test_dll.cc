// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some pointless code that will become a DLL with some exports and relocs.

#include <windows.h>

namespace {

void (*g_somestate)(int) = nullptr;
volatile int g_somevalue = 0;

}  // namespace

extern "C"
void DummyExport(int foo) {
  // The test will patch the code of this function. Do a volatile store to a
  // global variable to defeat LLVM's dead store elimination and the linker's
  // ICF. Otherwise the code patch affects code run during DLL unloading.
  // http://crbug.com/636157
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
