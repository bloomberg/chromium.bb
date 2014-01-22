// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NONSFI_IRT_UTIL_H_
#define COMPONENTS_NACL_LOADER_NONSFI_IRT_UTIL_H_

#include <errno.h>

namespace nacl {
namespace nonsfi {

// For IRT implementation, the following simple pattern is commonly used.
//
//   if (syscall(...) < 0)
//     return errno;
//   return 0;
//
// This function is a utility to write it in a line as follows:
//
//   return CheckError(syscall(...));
inline int CheckError(int result) {
  return result < 0 ? errno : 0;
}

// For IRT implementation, another but a similar pattern is also commonly used.
//
//   T result = syscall(...);
//   if (result < 0)
//     return errno;
//   *output = result;
//   return 0;
//
// This function is a utility to write it in a line as follows:
//
//   retrun CheckErrorWithResult(syscall(...), output);
//
// Here, T1 must be a signed integer value, and T2 must be convertible from
// T1.
template<typename T1, typename T2>
int CheckErrorWithResult(const T1& result, T2* out) {
  if (result < 0)
    return errno;

  *out = result;
  return 0;
}

}  // namespace nonsfi
}  // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_NONSFI_IRT_UTIL_H_
