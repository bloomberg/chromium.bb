// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_NAMED_PLATFORM_HANDLE_UTILS_H_
#define MOJO_EDK_EMBEDDER_NAMED_PLATFORM_HANDLE_UTILS_H_

#include "build/build_config.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"

namespace mojo {
namespace edk {

struct NamedPlatformHandle;

// Creates a client platform handle from |handle|. This may block until |handle|
// is ready to receive connections.
MOJO_SYSTEM_IMPL_EXPORT ScopedPlatformHandle
CreateClientHandle(const NamedPlatformHandle& handle);

// Creates a server platform handle from |handle|. On Windows, if
// |enforce_uniqueness| is true, this will fail if another pipe with the same
// name exists. On other platforms, |enforce_uniqueness| must be false.
MOJO_SYSTEM_IMPL_EXPORT ScopedPlatformHandle
CreateServerHandle(const NamedPlatformHandle& handle, bool enforce_uniqueness);

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_NAMED_PLATFORM_HANDLE_UTILS_H_
