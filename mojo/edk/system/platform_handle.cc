// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/platform_handle.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_FUCHSIA)
#include <unistd.h>
#include <zircon/status.h>
#include <zircon/syscalls.h>
#elif defined(OS_POSIX)
#include <unistd.h>
#endif

#include "base/logging.h"

#if defined(OS_WIN)
#include "base/optional.h"
#include "mojo/edk/system/scoped_process_handle.h"
#endif

namespace mojo {
namespace edk {

void InternalPlatformHandle::CloseIfNecessary() {
  if (!is_valid())
    return;

#if defined(OS_WIN)
  bool success = !!CloseHandle(handle);
  DPCHECK(success);
  handle = INVALID_HANDLE_VALUE;
#elif defined(OS_FUCHSIA)
  if (handle != ZX_HANDLE_INVALID) {
    zx_status_t result = zx_handle_close(handle);
    DCHECK_EQ(ZX_OK, result) << "CloseIfNecessary(zx_handle_close): "
                             << zx_status_get_string(result);
    handle = ZX_HANDLE_INVALID;
  }
  if (fd >= 0) {
    bool success = (close(fd) == 0);
    DPCHECK(success);
    fd = -1;
  }
#elif defined(OS_POSIX)
  if (type == Type::POSIX) {
    bool success = (close(handle) == 0);
    DPCHECK(success);
    handle = -1;
  }
#if defined(OS_MACOSX) && !defined(OS_IOS)
  else if (type == Type::MACH) {
    kern_return_t rv = mach_port_deallocate(mach_task_self(), port);
    DPCHECK(rv == KERN_SUCCESS);
    port = MACH_PORT_NULL;
  }
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
#else
#error "Platform not yet supported."
#endif  // defined(OS_WIN)
}

}  // namespace edk
}  // namespace mojo
