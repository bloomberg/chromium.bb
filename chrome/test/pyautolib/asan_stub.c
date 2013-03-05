// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Fake Address Sanitizer run-time support.
// Enough for programs to link and run, but will not find any errors.
// Also, linking this to shared libraries voids the warranty.
//
// We need this fake thunks if we build Chrome with ASAN support because
// pyautolib DSO is instrumented with ASAN and is loaded to regular python
// process that has no ASAN runtime.
//
// We have three options here:
// 1) use our custom build of Python that have ASAN runtime linked in,
// 2) do not instrument pyautolib with ASAN support,
// 3) use this fake asan thunks linked to pyautolib so that instrumented code
//    does not complain.
//
// Note that we should not use real ASAN runtime linked with DSO because it will
// not have information about memory allocations made prior to DSO load.
//
// Option (2) is not easy because pyautolib uses code from Chrome
// (see chrome_tests.gypi, dependencies for target_name: pyautolib) that
// has been instrumented with ASAN. So even if we disable -sanitize=address
// for pyautolib own sources, ASAN instrumentation will creep in from there.
// To avoid ASAN instrumentation, we might force Chrome build to compile all our
// dependencies one more time without -fsanitize=address.
//
// Note also that using these empty stubs prevents ASAN from catching bugs in
// Python-pyautolib process. But we do not need it, we are interested in Chrome
// bugs.


#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

void __asan_init() {
  static int inited = 0;
  if (inited) return;
  inited = 1;
#if __WORDSIZE == 64
  unsigned long start = 0x100000000000;
  unsigned long size  = 0x100000000000;
#else
  unsigned long start = 0x20000000;
  unsigned long size = 0x20000000;
#endif
  void *res = mmap((void*)start, size,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON | MAP_FIXED | MAP_NORESERVE,
                   0, 0);
  if (res == (void*)start) {
    fprintf(stderr, "Fake AddressSanitizer run-time initialized ok at %p\n",
            res);
  } else {
    fprintf(stderr, "Fake AddressSanitizer run-time failed to initialize.\n"
            "You have been warned. Aborting.");
    abort();
  }
}

void __asan_handle_no_return() { }
void __asan_init_v1() { }
void __asan_register_globals() { }
void __asan_report_load1() { }
void __asan_report_load16() { }
void __asan_report_load2() { }
void __asan_report_load4() { }
void __asan_report_load8() { }
void __asan_report_store1() { }
void __asan_report_store16() { }
void __asan_report_store2() { }
void __asan_report_store4() { }
void __asan_report_store8() { }
void __asan_set_error_report_callback() { }
void __asan_unregister_globals() { }
void __sanitizer_sandbox_on_notify() { }
