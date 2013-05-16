// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

extern "C" {

// This entry point and file is used to work around circular dependencies
// between the split DLLs. The CRT initialization will fail if done at attach
// time. Instead, we defer initialization until after both DLLs are loaded and
// then manually call the CRT initialization function (via DoDeferredCrtInit).
//
// ChromeEmptyEntry is the DLL's entry point.

BOOL WINAPI _DllMainCRTStartup(HINSTANCE, DWORD, LPVOID);

BOOL WINAPI ChromeEmptyEntry(HINSTANCE hinstance,
                             DWORD reason,
                             LPVOID reserved) {
  // We are going to do the 'process attach initialization manually via
  // DoDeferredCrtInit but we need to let the DLL_THREAD_ATTACH calls work.
  if (reason == DLL_PROCESS_ATTACH)
    return 1;
  return _DllMainCRTStartup(hinstance, reason, reserved);
}

__declspec(dllexport) BOOL __stdcall DoDeferredCrtInit(HINSTANCE hinstance) {
  return _DllMainCRTStartup(hinstance, DLL_PROCESS_ATTACH, NULL);
}

// This function is needed for the linker to succeed. It seems to pick the
// CRT lib based on the existence of "DllMain", and since we're renaming that,
// it instead chooses the one that links against "main". This function should
// never be called.
int main() {
  __debugbreak();
}

}
