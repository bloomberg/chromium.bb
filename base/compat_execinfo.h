// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A file you can include instead of <execinfo.h> if your project might have
// been compiled with our SUPPORT_MACOSX_10_4 flag defined.
// If SUPPORT_MACOSX_10_4 is not defined it just includes execinfo.h as normal,
// otherwise it defines the symbols itself as weak linked imports, which enables
// launching on 10.4 where they are not defined.

#ifndef BASE_COMPAT_EXECINFO_H
#define BASE_COMPAT_EXECINFO_H

#ifdef SUPPORT_MACOSX_10_4
// Manually define these here as weak imports, rather than including execinfo.h.
// This lets us launch on 10.4 which does not have these calls.
extern "C" {
  extern int backtrace(void**, int) __attribute__((weak_import));
  extern char** backtrace_symbols(void* const*, int)
      __attribute__((weak_import));
  extern void backtrace_symbols_fd(void* const*, int, int)
      __attribute__((weak_import));
}
#else
#include <execinfo.h>
#endif

#endif  //  BASE_COMPAT_EXECINFO_H

