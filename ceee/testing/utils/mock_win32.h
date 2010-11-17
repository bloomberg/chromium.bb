// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Win32 mocks.

#ifndef CEEE_TESTING_UTILS_MOCK_WIN32_H_
#define CEEE_TESTING_UTILS_MOCK_WIN32_H_

// atlwin.h re#defines SetWindowLongPtr and it caused a build error here.
#ifdef __ATLWIN_H__
#error "Do not include <atlwin.h> ahead of this."
#endif

#include <windows.h>

#include "ceee/testing/sidestep/auto_testing_hook.h"
#include "ceee/testing/utils/mock_static.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace testing {

// Mock object for some Kernel32 functions.
MOCK_STATIC_CLASS_BEGIN(MockKernel32)
  MOCK_STATIC_INIT_BEGIN(MockKernel32)
    MOCK_STATIC_INIT(GetModuleFileName);
    MOCK_STATIC_INIT(GetNativeSystemInfo);
    MOCK_STATIC_INIT(OpenThread);
    MOCK_STATIC_INIT(GetCommandLine);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC3(DWORD, WINAPI, GetModuleFileName, HMODULE, LPTSTR, DWORD);
  MOCK_STATIC1(void, WINAPI, GetNativeSystemInfo, LPSYSTEM_INFO);
  MOCK_STATIC3(HANDLE, CALLBACK, OpenThread, DWORD, BOOL, DWORD);
  MOCK_STATIC0(LPTSTR, WINAPI, GetCommandLine);
MOCK_STATIC_CLASS_END(MockKernel32)

// Separate from MockKernel32 since here be dragons.
// First off, putting the LoadLibrary functions with the rest of MockUser32
// is not a good idea since we want to avoid hooking these functions whenever
// the others are used. Also, for some undetermined reason, they just don't
// work when coupled in the MockUser32 class. So chances are you cannot use both
// MockLoadLibrary and MockUser32 in the same test...
MOCK_STATIC_CLASS_BEGIN(MockLoadLibrary)
  MOCK_STATIC_INIT_BEGIN(MockLoadLibrary)
    MOCK_STATIC_INIT(LoadLibrary);
    MOCK_STATIC_INIT(GetProcAddress);
    MOCK_STATIC_INIT(FreeLibrary);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC1(HINSTANCE, WINAPI, LoadLibrary, LPCTSTR);
  MOCK_STATIC2(FARPROC, WINAPI, GetProcAddress, HMODULE, LPCSTR);
  MOCK_STATIC1(BOOL, WINAPI, FreeLibrary, HMODULE);
MOCK_STATIC_CLASS_END(MockLoadLibrary)

// Mock object for some User32 functions.
MOCK_STATIC_CLASS_BEGIN(MockUser32)
  MOCK_STATIC_INIT_BEGIN(MockUser32)
    MOCK_STATIC_INIT(DefWindowProc);
    MOCK_STATIC_INIT(EnumChildWindows);
    MOCK_STATIC_INIT(EnumWindows);
    MOCK_STATIC_INIT(IsWindow);
    MOCK_STATIC_INIT(IsWindowVisible);
    MOCK_STATIC_INIT(GetClassName);
    MOCK_STATIC_INIT(GetForegroundWindow);
    MOCK_STATIC_INIT(GetParent);
    MOCK_STATIC_INIT(GetTopWindow);
    MOCK_STATIC_INIT(GetWindow);
    MOCK_STATIC_INIT(GetWindowRect);
    MOCK_STATIC_INIT(GetWindowThreadProcessId);
    MOCK_STATIC_INIT(KillTimer);
    MOCK_STATIC_INIT(MoveWindow);
    MOCK_STATIC_INIT(PostMessage);
    MOCK_STATIC_INIT(SendMessage);
    MOCK_STATIC_INIT(SetProp);
    MOCK_STATIC_INIT(SetTimer);
    MOCK_STATIC_INIT(SetWindowLongPtr);
    MOCK_STATIC_INIT(SetWindowPos);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC4(LRESULT, CALLBACK, DefWindowProc, HWND, UINT, WPARAM, LPARAM);
  MOCK_STATIC3(BOOL, CALLBACK, EnumChildWindows, HWND, WNDENUMPROC, LPARAM);
  MOCK_STATIC2(BOOL, CALLBACK, EnumWindows, WNDENUMPROC, LPARAM);
  MOCK_STATIC1(BOOL, CALLBACK, IsWindow, HWND);
  MOCK_STATIC1(BOOL, CALLBACK, IsWindowVisible, HWND);
  MOCK_STATIC3(int, CALLBACK, GetClassName, HWND, LPTSTR, int);
  MOCK_STATIC0(HWND, CALLBACK, GetForegroundWindow);
  MOCK_STATIC1(HWND, CALLBACK, GetParent, HWND);
  MOCK_STATIC1(HWND, CALLBACK, GetTopWindow, HWND);
  MOCK_STATIC2(HWND, CALLBACK, GetWindow, HWND, UINT);
  MOCK_STATIC2(BOOL, CALLBACK, GetWindowRect, HWND, LPRECT);
  MOCK_STATIC2(DWORD, CALLBACK, GetWindowThreadProcessId, HWND, LPDWORD);
  MOCK_STATIC2(BOOL, CALLBACK, KillTimer, HWND, UINT_PTR);
  MOCK_STATIC6(BOOL, CALLBACK, MoveWindow,
               HWND, int, int, int, int, BOOL);
  MOCK_STATIC4(BOOL, CALLBACK, PostMessage, HWND, UINT, WPARAM, LPARAM);
  MOCK_STATIC4(LRESULT, CALLBACK, SendMessage, HWND, UINT, WPARAM, LPARAM);
  MOCK_STATIC3(BOOL, CALLBACK, SetProp, HWND, LPCWSTR, HANDLE);
  MOCK_STATIC4(UINT_PTR, CALLBACK, SetTimer, HWND, UINT_PTR, UINT, TIMERPROC);
  MOCK_STATIC3(LONG_PTR, CALLBACK, SetWindowLongPtr, HWND, int, LONG_PTR);
  MOCK_STATIC7(BOOL, CALLBACK, SetWindowPos,
               HWND, HWND, int, int, int, int, UINT);
MOCK_STATIC_CLASS_END(MockUser32)

// Static functions for (effectively) mocking GetNativeSystemInfo
class NativeSystemInfoMockTool {
 public:
  static void SetEnvironment_x86(LPSYSTEM_INFO psys_info) {
    memset(psys_info, 0, sizeof(*psys_info));
    psys_info->wProcessorArchitecture = PROCESSOR_ARCHITECTURE_INTEL;
    psys_info->dwNumberOfProcessors = 1;
  }

  static void SetEnvironment_x64(LPSYSTEM_INFO psys_info) {
    memset(psys_info, 0, sizeof(*psys_info));
    psys_info->wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
    psys_info->dwNumberOfProcessors = 4;
  }
};

}  // namespace testing

#endif  // CEEE_TESTING_UTILS_MOCK_WIN32_H_
