// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/memory.h"

#include <limits>

#include "base/logging.h"

namespace mojo {
namespace system {

template <size_t size>
bool VerifyUserPointerForSize(const void* pointer, size_t count) {
  if (count > std::numeric_limits<size_t>::max() / size)
    return false;

  // TODO(vtl): If running in kernel mode, do a full verification. For now, just
  // check that it's non-null if |size| is nonzero. (A faster user mode
  // implementation is also possible if this check is skipped.)
  return count == 0 || !!pointer;
}

// Explicitly instantiate the sizes we need. Add instantiations as needed.
template MOJO_SYSTEM_EXPORT bool VerifyUserPointerForSize<1>(
    const void*, size_t);
template MOJO_SYSTEM_EXPORT bool VerifyUserPointerForSize<4>(
    const void*, size_t);

}  // namespace system
}  // namespace mojo
