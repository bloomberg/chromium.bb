// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_POSIX_HOOKS_INTERNAL_H_
#define BASE_FILES_FILE_POSIX_HOOKS_INTERNAL_H_

#include "base/base_export.h"

namespace base {

// Define empty hooks for blacklisting file descriptors used in base::File.
// These functions should be declared 'weak', i.e. the functions declared in
// a default way would have precedence over the weak ones at link time. This
// works for both static and dynamic linking.
// TODO(pasko): Remove these hooks when crbug.com/424562 is fixed.
//
// With compilers other than GCC/Clang define strong no-op symbols for
// simplicity.
#if defined(COMPILER_GCC)
#define ATTRIBUTE_WEAK __attribute__ ((weak))
#else
#define ATTRIBUTE_WEAK
#endif
BASE_EXPORT void ProtectFileDescriptor(int fd) ATTRIBUTE_WEAK;
BASE_EXPORT void UnprotectFileDescriptor(int fd) ATTRIBUTE_WEAK;
#undef ATTRIBUTE_WEAK

}  // namespace base

#endif  // BASE_FILES_FILE_POSIX_HOOKS_INTERNAL_H_
