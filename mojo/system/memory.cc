// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/memory.h"

#include <limits>

#include "base/logging.h"

namespace mojo {
namespace system {

bool VerifyUserPointer(const void* pointer, size_t count, size_t size_each) {
  DCHECK_GT(size_each, 0u);
  if (count > std::numeric_limits<size_t>::max() / size_each)
    return false;

  // TODO(vtl): If running in kernel mode, do a full verification. For now, just
  // check that it's non-null if |size| is nonzero. (A faster user mode
  // implementation is also possible if this check is skipped.)
  return count == 0 || !!pointer;
}

}  // namespace system
}  // namespace mojo
