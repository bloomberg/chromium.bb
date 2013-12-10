// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MEMORY_H_
#define MOJO_SYSTEM_MEMORY_H_

#include <stddef.h>

#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// This is just forward-declared, with the definition and explicit
// instantiations in the .cc file. This is used by |VerifyUserPointer<T>()|
// below, and you should use that instead.
template <size_t size>
bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerForSize(const void* pointer,
                                                      size_t count);

// Verify that |count * sizeof(T)| bytes can be read from the user |pointer|
// insofar as possible/necessary (note: this is done carefully since |count *
// sizeof(T)| may overflow a |size_t|. |count| may be zero. If |T| is |void|,
// then the size of each element is taken to be a single byte.
//
// For example, if running in kernel mode, this should be a full verification
// that the given memory is owned and readable by the user process. In user
// mode, if crashes are acceptable, this may do nothing at all (and always
// return true).
template <typename T>
bool VerifyUserPointer(const T* pointer, size_t count) {
  return VerifyUserPointerForSize<sizeof(T)>(pointer, count);
}

// Special-case |T| equals |void| so that the size is in bytes, as indicated
// above.
template <>
inline bool VerifyUserPointer<void>(const void* pointer, size_t count) {
  return VerifyUserPointerForSize<1>(pointer, count);
}

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MEMORY_H_
