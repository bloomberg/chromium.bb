// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/platform_handle_utils.h"

#include <unistd.h>

#include "base/logging.h"

namespace mojo {
namespace edk {

ScopedInternalPlatformHandle DuplicatePlatformHandle(
    InternalPlatformHandle platform_handle) {
  DCHECK(platform_handle.is_valid());
  // Note that |dup()| returns -1 on error (which is exactly the value we use
  // for invalid |InternalPlatformHandle| FDs).
  InternalPlatformHandle duped(dup(platform_handle.handle));
  duped.needs_connection = platform_handle.needs_connection;
  return ScopedInternalPlatformHandle(duped);
}

}  // namespace edk
}  // namespace mojo
