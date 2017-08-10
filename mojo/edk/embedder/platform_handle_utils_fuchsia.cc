// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/platform_handle_utils.h"

#include "base/logging.h"

namespace mojo {
namespace edk {

ScopedPlatformHandle DuplicatePlatformHandle(PlatformHandle platform_handle) {
  DCHECK(platform_handle.is_valid());
  mx_handle_t duped;
  // mx_handle_duplicate won't touch |duped| in case of failure.
  mx_status_t result = mx_handle_duplicate(platform_handle.as_handle(),
                                           MX_RIGHT_SAME_RIGHTS, &duped);
  DLOG_IF(ERROR, result != MX_OK) << "mx_duplicate_handle failed: " << result;
  return ScopedPlatformHandle(PlatformHandle::ForHandle(duped));
}

}  // namespace edk
}  // namespace mojo
