// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/delay_load_hook_win.h"

#include <DelayIMP.h>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

// So long as these symbols are supplied to the final binary through an
// object file (as opposed to indirectly through a library), these pointers
// will override the CRT's symbols and direct the notifications to our hook.
// Alternatively referencing the ChromeDelayLoadHook function somehow will
// cause this declaration of these variables to take preference to the delay
// load runtime's defaults (in delayimp.lib).
#if _MSC_FULL_VER >= 190024112
// Prior to Visual Studio 2015 Update 3, these hooks were non-const.  They were
// made const to improve security (global, writable function pointers are bad).
// This #ifdef is needed for testing of VS 2015 Update 3 pre-release and can be
// removed when we formally switch to Update 3 or higher.
// TODO(612313): remove the #if when we update toolchains.
const PfnDliHook __pfnDliNotifyHook2 = ChromeDelayLoadHook;
const PfnDliHook __pfnDliFailureHook2 = ChromeDelayLoadHook;
#else
PfnDliHook __pfnDliNotifyHook2 = ChromeDelayLoadHook;
PfnDliHook __pfnDliFailureHook2 = ChromeDelayLoadHook;
#endif


namespace {

FARPROC OnPreLoadLibrary(DelayLoadInfo* info) {
  // If the DLL name ends with "-delay.dll", this call is about one of our
  // custom import libraries. In this case we need to snip the suffix off,
  // and bind to the real DLL.
  std::string dll_name(info->szDll);
  const char kDelaySuffix[] = "-delay.dll";
  if (base::EndsWith(dll_name, kDelaySuffix,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    // Trim the "-delay.dll" suffix from the string.
    dll_name.resize(dll_name.length() - (sizeof(kDelaySuffix) - 1));
    dll_name.append(".dll");

    return reinterpret_cast<FARPROC>(::LoadLibraryA(dll_name.c_str()));
  }

  return NULL;
}

}  // namespace

// This function is a delay load notification hook. It is invoked by the
// delay load support in the visual studio runtime.
// See http://msdn.microsoft.com/en-us/library/z9h1h6ty(v=vs.100).aspx for
// details.
extern "C" FARPROC WINAPI ChromeDelayLoadHook(unsigned reason,
                                              DelayLoadInfo* info) {
  switch (reason) {
    case dliNoteStartProcessing:
    case dliNoteEndProcessing:
      // Nothing to do here.
      break;

    case dliNotePreLoadLibrary:
      return OnPreLoadLibrary(info);
      break;

    case dliNotePreGetProcAddress:
      // Nothing to do here.
      break;

    case dliFailLoadLib:
    case dliFailGetProc:
      // Returning NULL from error notifications will cause the delay load
      // runtime to raise a VcppException structured exception, that some code
      // might want to handle.
      return NULL;
      break;

    default:
      NOTREACHED() << "Impossible delay load notification.";
      break;
  }

  // Returning NULL causes the delay load machinery to perform default
  // processing for this notification.
  return NULL;
}
