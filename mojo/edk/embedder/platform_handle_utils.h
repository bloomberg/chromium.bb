// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_UTILS_H_
#define MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_UTILS_H_

#include "base/memory/platform_shared_memory_region.h"
#include "mojo/edk/embedder/platform_handle.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace edk {

// Closes all the |PlatformHandle|s in the given container.
template <typename PlatformHandleContainer>
MOJO_SYSTEM_IMPL_EXPORT inline void CloseAllPlatformHandles(
    PlatformHandleContainer* platform_handles) {
  for (typename PlatformHandleContainer::iterator it =
           platform_handles->begin();
       it != platform_handles->end(); ++it)
    it->CloseIfNecessary();
}

MOJO_SYSTEM_IMPL_EXPORT MojoResult MojoPlatformHandleToScopedPlatformHandle(
    const MojoPlatformHandle* platform_handle,
    ScopedPlatformHandle* out_handle);

MOJO_SYSTEM_IMPL_EXPORT MojoResult
ScopedPlatformHandleToMojoPlatformHandle(ScopedPlatformHandle handle,
                                         MojoPlatformHandle* platform_handle);

// Duplicates the given |PlatformHandle| (which must be valid). (Returns an
// invalid |ScopedPlatformHandle| on failure.)
MOJO_SYSTEM_IMPL_EXPORT ScopedPlatformHandle
DuplicatePlatformHandle(PlatformHandle platform_handle);

// Converts a base shared memory platform handle into one (maybe two on POSIX)
// EDK ScopedPlatformHandles.
MOJO_SYSTEM_IMPL_EXPORT void ExtractPlatformHandlesFromSharedMemoryRegionHandle(
    base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle handle,
    ScopedPlatformHandle* extracted_handle,
    ScopedPlatformHandle* extracted_readonly_handle);

// Converts one (maybe two on POSIX) EDK ScopedPlatformHandles to a base
// shared memory platform handle.
MOJO_SYSTEM_IMPL_EXPORT
base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle
CreateSharedMemoryRegionHandleFromPlatformHandles(
    ScopedPlatformHandle handle,
    ScopedPlatformHandle readonly_handle);

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_PLATFORM_HANDLE_UTILS_H_
