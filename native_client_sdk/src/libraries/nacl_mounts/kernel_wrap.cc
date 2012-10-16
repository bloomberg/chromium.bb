/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/types.h>  // Include something that will define __GLIBC__.

#if defined(__native_client__)
#  if defined(__GLIBC__)
#    include "kernel_wrap_glibc.cc"
#  else  // !__GLIBC__
#    include "kernel_wrap_newlib.cc"
#  endif
#elif defined(WIN32)
#  include "kernel_wrap_win.cc"
#else
#  error Kernel wrapping not supported on your platform!
#endif
