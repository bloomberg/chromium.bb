// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/memory.h"

#include <limits>

#include "base/logging.h"

namespace mojo {
namespace system {

namespace internal {

template <size_t size, size_t alignment>
bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerHelper(const void* pointer) {
  // TODO(vtl): If running in kernel mode, do a full verification. For now, just
  // check that it's non-null and aligned. (A faster user mode implementation is
  // also possible if this check is skipped.)
  return !!pointer && reinterpret_cast<uintptr_t>(pointer) % alignment == 0;
}

// Explicitly instantiate the sizes we need. Add instantiations as needed.
template MOJO_SYSTEM_IMPL_EXPORT bool VerifyUserPointerHelper<1, 1>(
    const void*);
template MOJO_SYSTEM_IMPL_EXPORT bool VerifyUserPointerHelper<4, 4>(
    const void*);
template MOJO_SYSTEM_IMPL_EXPORT bool VerifyUserPointerHelper<8, 8>(
    const void*);

template <size_t size, size_t alignment>
bool VerifyUserPointerWithCountHelper(const void* pointer, size_t count) {
  if (count > std::numeric_limits<size_t>::max() / size)
    return false;

  // TODO(vtl): If running in kernel mode, do a full verification. For now, just
  // check that it's non-null and aligned if |count| is nonzero. (A faster user
  // mode implementation is also possible if this check is skipped.)
  return count == 0 ||
         (!!pointer && reinterpret_cast<uintptr_t>(pointer) % alignment == 0);
}

// Explicitly instantiate the sizes we need. Add instantiations as needed.
template MOJO_SYSTEM_IMPL_EXPORT bool VerifyUserPointerWithCountHelper<1, 1>(
    const void*, size_t);
template MOJO_SYSTEM_IMPL_EXPORT bool VerifyUserPointerWithCountHelper<4, 4>(
    const void*, size_t);
template MOJO_SYSTEM_IMPL_EXPORT bool VerifyUserPointerWithCountHelper<8, 8>(
    const void*, size_t);

}  // nameespace internal

template <size_t alignment>
bool VerifyUserPointerWithSize(const void* pointer, size_t size) {
  // TODO(vtl): If running in kernel mode, do a full verification. For now, just
  // check that it's non-null and aligned. (A faster user mode implementation is
  // also possible if this check is skipped.)
  return size == 0 ||
         (!!pointer && reinterpret_cast<uintptr_t>(pointer) % alignment == 0);
}

// Explicitly instantiate the alignments we need. Add instantiations as needed.
template MOJO_SYSTEM_IMPL_EXPORT bool VerifyUserPointerWithSize<1>(const void*,
                                                                   size_t);
template MOJO_SYSTEM_IMPL_EXPORT bool VerifyUserPointerWithSize<4>(const void*,
                                                                   size_t);
template MOJO_SYSTEM_IMPL_EXPORT bool VerifyUserPointerWithSize<8>(const void*,
                                                                   size_t);

}  // namespace system
}  // namespace mojo
