// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#if defined(_WIN32_WINNT_WIN8) && _MSC_VER < 1700
// The Windows 8 SDK defines FACILITY_VISUALCPP in winerror.h, and in
// delayimp.h previous to VS2012.
#undef FACILITY_VISUALCPP
#endif
#include <DelayIMP.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/scoped_native_library.h"
#include "chrome/app/delay_load_hook_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ChromeDelayLoadHookTest : public testing::Test {
 public:
  ChromeDelayLoadHookTest() : proc_ptr_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    SetupInfo("kernel32.dll");
  }

  void SetupInfo(const char* dll_name) {
    info_.cb = sizeof(info_);
    info_.pidd =  NULL;
    info_.ppfn = &proc_ptr_;
    info_.szDll = dll_name;
    info_.dlp.fImportByName = TRUE;
    info_.dlp.szProcName = "CreateFileA";
    info_.hmodCur = NULL;
    info_.pfnCur = NULL;
    info_.dwLastError = 0;
  }

  FARPROC proc_ptr_;
  DelayLoadInfo info_;
};

}  // namespace

TEST_F(ChromeDelayLoadHookTest, HooksAreSetAtLinkTime) {
  // This test verifies that referencing the ChromeDelayLoadHook at link
  // time results in overriding the delay loader's hook instances in the CRT
  // ropriately.
  EXPECT_TRUE(__pfnDliNotifyHook2 == ChromeDelayLoadHook);
  EXPECT_TRUE(__pfnDliFailureHook2 == ChromeDelayLoadHook);
}

TEST_F(ChromeDelayLoadHookTest, NonSuffixedDllsAreNotHandled) {
  // The hook should ignore non-suffixed DLLs.
  SetupInfo("kernel32.dll");

  HMODULE none = reinterpret_cast<HMODULE>(
      ChromeDelayLoadHook(dliNotePreLoadLibrary, &info_));
   // Make sure the library is released on exit.
   base::ScopedNativeLibrary lib_holder(none);

   ASSERT_TRUE(none == NULL);
}

TEST_F(ChromeDelayLoadHookTest, SuffixedDllsAreRedirected) {
  // Check that a DLL name of the form "foo-delay.dll" gets redirected to
  // the "foo.dll".
  SetupInfo("kernel32-delay.dll");
  HMODULE kernel32 = reinterpret_cast<HMODULE>(
      ChromeDelayLoadHook(dliNotePreLoadLibrary, &info_));

   // Make sure the library is released on exit.
   base::ScopedNativeLibrary lib_holder(kernel32);

   ASSERT_TRUE(kernel32 == ::GetModuleHandle(L"kernel32.dll"));
}

TEST_F(ChromeDelayLoadHookTest, IgnoresUnhandledNotifications) {
  SetupInfo("kernel32-delay.dll");

  // The hook should ignore all notifications but the preload notifications.
  EXPECT_TRUE(ChromeDelayLoadHook(dliNoteStartProcessing, &info_) == NULL);
  EXPECT_TRUE(ChromeDelayLoadHook(dliNotePreGetProcAddress, &info_) == NULL);
  EXPECT_TRUE(ChromeDelayLoadHook(dliNoteEndProcessing, &info_) == NULL);
  EXPECT_TRUE(ChromeDelayLoadHook(dliFailLoadLib, &info_) == NULL);
  EXPECT_TRUE(ChromeDelayLoadHook(dliFailGetProc, &info_) == NULL);
}
