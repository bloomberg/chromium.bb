// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Main entry point for a DLL that can be instructed to crash on
// load or unload by setting an environment variable appropriately.
//
// Note: This code has no CRT to lean on, because some versions of the CRT
//    have a bug whereby they leave dangling state after taking an exception
//    during DLL_PROCESS_ATTACH. This in turn causes the loading process to
//    crash on exit. To work around this, this DLL has its entrypoint set
//    to the DllMain routine and does not link with the CRT.

#include <windows.h>

#include "crash_dll.h"

void Crash() {
  char* null_pointer = reinterpret_cast<char*>(kCrashAddress);

  *null_pointer = '\0';
}

void CrashConditionally(const wchar_t* name) {
  wchar_t value[1024];
  DWORD ret = ::GetEnvironmentVariable(name, value, ARRAYSIZE(value));
  if (ret != 0 || ERROR_ENVVAR_NOT_FOUND != ::GetLastError())
    Crash();
}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason,
                               LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH)
    CrashConditionally(kCrashOnLoadMode);
  else if (reason == DLL_PROCESS_DETACH)
    CrashConditionally(kCrashOnUnloadMode);

  return 1;
}
