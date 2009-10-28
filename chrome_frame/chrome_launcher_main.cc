// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome_frame/chrome_launcher.h"

// We want to keep this EXE tiny, so we avoid all dependencies and link to no
// libraries, and we do not use the C runtime.
//
// To catch errors in debug builds, we define an extremely simple assert macro.
#ifndef NDEBUG
#define CLM_ASSERT(x) do { if (!(x)) { ::DebugBreak(); } } while (false)
#else
#define CLM_ASSERT(x)
#endif  // NDEBUG

// In release builds, we skip the standard library completely to minimize
// size.  This is more work in debug builds, and unnecessary, hence the
// different signatures.
#ifndef NDEBUG
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, wchar_t*, int) {
#else
extern "C" void __cdecl WinMainCRTStartup() {
#endif  // NDEBUG
  // This relies on the chrome_launcher.exe residing in the same directory
  // as our DLL.  We build a full path to avoid loading it from any other
  // directory in the DLL search path.
  //
  // The code is a bit verbose because we can't use the standard library.
  const wchar_t kBaseName[] = L"npchrome_tab.dll";
  wchar_t file_path[MAX_PATH + (sizeof(kBaseName) / sizeof(kBaseName[0])) + 1];
  file_path[0] = L'\0';
  ::GetModuleFileName(::GetModuleHandle(NULL), file_path, MAX_PATH);

  // Find index of last slash, and null-terminate the string after it.
  //
  // Proof for security purposes, since we can't use the safe string
  // manipulation functions from the runtime:
  // - File_path is always null-terminated, by us initially and by 
  //   ::GetModuleFileName if it puts anything into the buffer.
  // - If there is no slash in the path then it's a relative path, not an
  //   absolute one, and the code ends up creating a relative path to
  //   npchrome_tab.dll.
  // - It's safe to use lstrcatW since we know the maximum length of both
  //   parts we are concatenating, and we know the buffer will fit them in
  //   the worst case.
  int slash_index = lstrlenW(file_path);
  // Invariant: 0 <= slash_index < MAX_PATH
  CLM_ASSERT(slash_index > 0);
  while (slash_index > 0 && file_path[slash_index] != L'\\')
    --slash_index;
  // Invariant: 0 <= slash_index < MAX_PATH and it is either the index of 
  // the last \ in the path, or 0.
  if (slash_index != 0)
    ++slash_index;  // don't remove the last '\'
  file_path[slash_index] = L'\0';

  lstrcatW(file_path, kBaseName);

  UINT exit_code = ERROR_FILE_NOT_FOUND;
  HMODULE chrome_tab = ::LoadLibrary(file_path);
  CLM_ASSERT(chrome_tab);
  if (chrome_tab) {
    chrome_launcher::CfLaunchChromeProc proc = 
        reinterpret_cast<chrome_launcher::CfLaunchChromeProc>(
            ::GetProcAddress(chrome_tab, "CfLaunchChrome"));
    CLM_ASSERT(proc);
    if (proc) {
      exit_code = proc();
    } else {
      exit_code = ERROR_INVALID_FUNCTION;
    }

    ::FreeLibrary(chrome_tab);
  }
  ::ExitProcess(exit_code);
}
