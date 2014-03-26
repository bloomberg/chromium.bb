// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the default options for various compiler-based dynamic
// tools.

#include "build/build_config.h"

// Functions returning default options are declared weak in the tools' runtime
// libraries. To make the linker pick the strong replacements for those
// functions from this module, we explicitly force its inclusion by passing
// -Wl,-u_sanitizer_options_link_helper
extern "C"
void _sanitizer_options_link_helper() { }

#if defined(ADDRESS_SANITIZER)
#if defined(OS_LINUX)
// Default AddressSanitizer options for the official build. These do not affect
// tests on buildbots (which don't set GOOGLE_CHROME_BUILD) or non-official
// Chromium builds.
//  - disable the strict memcmp() checking (http://crbug.com/178677 and
//    http://crbug.com/178404).
//  - set the malloc_context_size (i.e. the size of stack traces collected by
//    ASan for each malloc/free) to 5. These stack traces tend to accumulate
//    very fast in applications using JIT (v8 in Chrome's case), see
//    https://code.google.com/p/address-sanitizer/issues/detail?id=177
//  - disable the in-process symbolization, which isn't 100% compatible with
//    the existing sandboxes and doesn't make much sense for stripped official
//    binaries.
#if defined(GOOGLE_CHROME_BUILD)
const char kAsanDefaultOptions[] =
    "malloc_context_size=5 strict_memcmp=0 symbolize=false";
#else
const char *kAsanDefaultOptions = "strict_memcmp=0 symbolize=false";
#endif  // GOOGLE_CHROME_BUILD

#elif defined(OS_MACOSX)
const char *kAsanDefaultOptions = "strict_memcmp=0 replace_intrin=0";
#endif  // OS_LINUX

#if defined(OS_LINUX) || defined(OS_MACOSX)
extern "C"
__attribute__((no_sanitize_address))
const char *__asan_default_options() {
  return kAsanDefaultOptions;
}
#endif  // OS_LINUX || OS_MACOSX
#endif  // ADDRESS_SANITIZER
