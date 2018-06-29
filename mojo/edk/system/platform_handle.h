// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PLATFORM_HANDLE_H_
#define MOJO_EDK_SYSTEM_PLATFORM_HANDLE_H_

#include "build/build_config.h"
#include "mojo/edk/system/system_impl_export.h"

#if defined(OS_WIN)
#include <windows.h>

#include "base/process/process_handle.h"
#elif defined(OS_FUCHSIA)
#include <lib/fdio/limits.h>
#include <zircon/syscalls.h>
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include <mach/mach.h>
#endif

#include "base/logging.h"

namespace mojo {
namespace edk {

#if defined(OS_WIN)
struct MOJO_SYSTEM_IMPL_EXPORT InternalPlatformHandle {
  InternalPlatformHandle() : InternalPlatformHandle(INVALID_HANDLE_VALUE) {}
  explicit InternalPlatformHandle(HANDLE handle) : handle(handle) {}

  void CloseIfNecessary();

  bool is_valid() const { return handle != INVALID_HANDLE_VALUE; }

  HANDLE handle;

  // A Windows HANDLE may be an unconnected named pipe. In this case, we need to
  // wait for a connection before communicating on the pipe.
  bool needs_connection = false;
};
#elif defined(OS_FUCHSIA)
// TODO(fuchsia): Find a clean way to share this with the POSIX version.
// |zx_handle_t| is a typedef of |int|, so we only allow InternalPlatformHandle
// to be created via explicit For<type>() creator functions.
struct MOJO_SYSTEM_IMPL_EXPORT InternalPlatformHandle {
 public:
  static InternalPlatformHandle ForHandle(zx_handle_t handle) {
    InternalPlatformHandle platform_handle;
    platform_handle.handle = handle;
    return platform_handle;
  }
  static InternalPlatformHandle ForFd(int fd) {
    InternalPlatformHandle platform_handle;
    DCHECK_LT(fd, FDIO_MAX_FD);
    platform_handle.fd = fd;
    return platform_handle;
  }

  void CloseIfNecessary();
  bool is_valid() const { return is_valid_fd() || is_valid_handle(); }
  bool is_valid_handle() const { return handle != ZX_HANDLE_INVALID && fd < 0; }
  zx_handle_t as_handle() const { return handle; }
  bool is_valid_fd() const { return fd >= 0 && handle == ZX_HANDLE_INVALID; }
  int as_fd() const { return fd; }

 private:
  zx_handle_t handle = ZX_HANDLE_INVALID;
  int fd = -1;
};
#elif defined(OS_POSIX)
struct MOJO_SYSTEM_IMPL_EXPORT InternalPlatformHandle {
  InternalPlatformHandle() {}
  explicit InternalPlatformHandle(int handle) : handle(handle) {}
#if defined(OS_MACOSX) && !defined(OS_IOS)
  explicit InternalPlatformHandle(mach_port_t port)
      : type(Type::MACH), port(port) {}
#endif

  void CloseIfNecessary();

  bool is_valid() const {
#if defined(OS_MACOSX) && !defined(OS_IOS)
    if (type == Type::MACH)
      return port != MACH_PORT_NULL;
#endif
    return handle != -1;
  }

  enum class Type {
    POSIX,
#if defined(OS_MACOSX) && !defined(OS_IOS)
    MACH,
#endif
  };
  Type type = Type::POSIX;

  int handle = -1;

  // A POSIX handle may be a listen handle that can accept a connection.
  bool needs_connection = false;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  mach_port_t port = MACH_PORT_NULL;
#endif
};
#else
#error "Platform not yet supported."
#endif

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PLATFORM_HANDLE_H_
