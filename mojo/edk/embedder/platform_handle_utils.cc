// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/platform_handle_utils.h"

#include "build/build_config.h"

namespace mojo {
namespace edk {

MojoResult MojoPlatformHandleToScopedPlatformHandle(
    const MojoPlatformHandle* platform_handle,
    ScopedPlatformHandle* out_handle) {
  if (platform_handle->struct_size != sizeof(MojoPlatformHandle))
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (platform_handle->type == MOJO_PLATFORM_HANDLE_TYPE_INVALID) {
    out_handle->reset();
    return MOJO_RESULT_OK;
  }

  PlatformHandle handle;
  switch (platform_handle->type) {
#if defined(OS_FUCHSIA)
    case MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE:
      handle = PlatformHandle::ForHandle(platform_handle->value);
      break;
    case MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR:
      handle = PlatformHandle::ForFd(platform_handle->value);
      break;

#elif defined(OS_POSIX)
    case MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR:
      handle.handle = static_cast<int>(platform_handle->value);
      break;
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
    case MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT:
      handle.type = PlatformHandle::Type::MACH;
      handle.port = static_cast<mach_port_t>(platform_handle->value);
      break;
#endif

#if defined(OS_WIN)
    case MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE:
      handle.handle = reinterpret_cast<HANDLE>(platform_handle->value);
      break;
#endif

    default:
      return MOJO_RESULT_INVALID_ARGUMENT;
  }

  out_handle->reset(handle);
  return MOJO_RESULT_OK;
}

MojoResult ScopedPlatformHandleToMojoPlatformHandle(
    ScopedPlatformHandle handle,
    MojoPlatformHandle* platform_handle) {
  if (platform_handle->struct_size != sizeof(MojoPlatformHandle))
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (!handle.is_valid()) {
    platform_handle->type = MOJO_PLATFORM_HANDLE_TYPE_INVALID;
    return MOJO_RESULT_OK;
  }

#if defined(OS_FUCHSIA)
  if (handle.get().is_valid_fd()) {
    platform_handle->type = MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
    platform_handle->value = handle.release().as_fd();
  } else {
    platform_handle->type = MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE;
    platform_handle->value = handle.release().as_handle();
  }
#elif defined(OS_POSIX)
  switch (handle.get().type) {
    case PlatformHandle::Type::POSIX:
      platform_handle->type = MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
      platform_handle->value = static_cast<uint64_t>(handle.release().handle);
      break;

#if defined(OS_MACOSX) && !defined(OS_IOS)
    case PlatformHandle::Type::MACH:
      platform_handle->type = MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT;
      platform_handle->value = static_cast<uint64_t>(handle.release().port);
      break;
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

    default:
      return MOJO_RESULT_INVALID_ARGUMENT;
  }
#elif defined(OS_WIN)
  platform_handle->type = MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE;
  platform_handle->value = reinterpret_cast<uint64_t>(handle.release().handle);
#endif  // defined(OS_WIN)

  return MOJO_RESULT_OK;
}

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
