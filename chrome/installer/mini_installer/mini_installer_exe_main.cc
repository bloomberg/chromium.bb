// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "chrome/installer/mini_installer/mini_installer.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

extern "C" int __stdcall MainEntryPoint() {
  mini_installer::ProcessExitResult result =
      mini_installer::WMain(reinterpret_cast<HMODULE>(&__ImageBase));
  ::ExitProcess(result.exit_code);
}

#if defined(ADDRESS_SANITIZER)
// Executables instrumented with ASAN need CRT functions. We do not use
// the /ENTRY switch for ASAN instrumented executable and a "main" function
// is required.
extern "C" int WINAPI wWinMain(HINSTANCE /* instance */,
                               HINSTANCE /* previous_instance */,
                               LPWSTR /* command_line */,
                               int /* command_show */) {
  return MainEntryPoint();
}
#endif

// VC Express editions don't come with the memset CRT obj file and linking to
// the obj files between versions becomes a bit problematic. Therefore,
// simply implement memset.
//
// This also avoids having to explicitly set the __sse2_available hack when
// linking with both the x64 and x86 obj files which is required when not
// linking with the std C lib in certain instances (including Chromium) with
// MSVC.  __sse2_available determines whether to use SSE2 intructions with
// std C lib routines, and is set by MSVC's std C lib implementation normally.
extern "C" {
#pragma function(memset)
void* memset(void* dest, int c, size_t count) {
  uint8_t* scan = reinterpret_cast<uint8_t*>(dest);
  while (count--)
    *scan++ = static_cast<uint8_t>(c);
  return dest;
}
}  // extern "C"
