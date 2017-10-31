// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/protected_buffer_manager_proxy.h"

#include "chrome/gpu/protected_buffer_manager.h"
#include "mojo/public/cpp/system/platform_handle.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace chromeos {
namespace arc {

GpuArcProtectedBufferManagerProxy::GpuArcProtectedBufferManagerProxy(
    chromeos::arc::ProtectedBufferManager* protected_buffer_manager)
    : protected_buffer_manager_(protected_buffer_manager) {
  DCHECK(protected_buffer_manager_);
}

base::ScopedFD GpuArcProtectedBufferManagerProxy::UnwrapFdFromMojoHandle(
    mojo::ScopedHandle handle) {
  base::PlatformFile platform_file;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (mojo_result != MOJO_RESULT_OK) {
    VLOGF(1) << "UnwrapPlatformFile failed: " << mojo_result;
    return base::ScopedFD();
  }

  return base::ScopedFD(platform_file);
}

mojo::ScopedHandle GpuArcProtectedBufferManagerProxy::WrapFdInMojoHandle(
    base::ScopedFD fd) {
  return mojo::WrapPlatformFile(fd.release());
}

void GpuArcProtectedBufferManagerProxy::GetProtectedSharedMemoryFromHandle(
    mojo::ScopedHandle dummy_handle,
    GetProtectedSharedMemoryFromHandleCallback callback) {
  base::ScopedFD unwrapped_fd = UnwrapFdFromMojoHandle(std::move(dummy_handle));

  base::ScopedFD shmem_fd(
      protected_buffer_manager_
          ->GetProtectedSharedMemoryHandleFor(std::move(unwrapped_fd))
          .Release());

  std::move(callback).Run(WrapFdInMojoHandle(std::move(shmem_fd)));
}

}  // namespace arc
}  // namespace chromeos
