// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/platform_handle_utils.h"

#include "build/build_config.h"

namespace mojo {
namespace edk {

void ExtractPlatformHandlesFromSharedMemoryRegionHandle(
    base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle handle,
    ScopedPlatformHandle* extracted_handle,
    ScopedPlatformHandle* extracted_readonly_handle) {
#if defined(OS_WIN)
  extracted_handle->reset(PlatformHandle(handle.Take()));
#elif defined(OS_FUCHSIA)
  extracted_handle->reset(PlatformHandle::ForHandle(handle.release()));
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  // This is a Mach port. Same code as below, but separated for clarity.
  extracted_handle->reset(PlatformHandle(handle.release()));
#elif defined(OS_ANDROID)
  // This is a file descriptor. Same code as above, but separated for clarity.
  extracted_handle->reset(PlatformHandle(handle.release()));
#else
  extracted_handle->reset(PlatformHandle(handle.fd.release()));
  extracted_readonly_handle->reset(
      PlatformHandle(handle.readonly_fd.release()));
#endif
}

base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle
CreateSharedMemoryRegionHandleFromPlatformHandles(
    ScopedPlatformHandle handle,
    ScopedPlatformHandle readonly_handle) {
#if defined(OS_WIN)
  DCHECK(!readonly_handle.is_valid());
  return base::win::ScopedHandle(handle.release().handle);
#elif defined(OS_FUCHSIA)
  DCHECK(!readonly_handle.is_valid());
  return base::ScopedZxHandle(handle.release().as_handle());
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  DCHECK(!readonly_handle.is_valid());
  return base::mac::ScopedMachSendRight(handle.release().port);
#elif defined(OS_ANDROID)
  DCHECK(!readonly_handle.is_valid());
  return base::ScopedFD(handle.release().handle);
#else
  return base::subtle::ScopedFDPair(
      base::ScopedFD(handle.release().handle),
      base::ScopedFD(readonly_handle.release().handle));
#endif
}

}  // namespace edk
}  // namespace mojo
