// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_PROTECTED_BUFFER_MANAGER_PROXY_H_
#define CHROME_GPU_PROTECTED_BUFFER_MANAGER_PROXY_H_

#include "components/arc/common/protected_buffer_manager.mojom.h"

namespace chromeos {
namespace arc {

class ProtectedBufferManager;

// Manages mojo IPC translation for chromeos::arc::ProtectedBufferManager.
class GpuArcProtectedBufferManagerProxy
    : public ::arc::mojom::ProtectedBufferManager {
 public:
  explicit GpuArcProtectedBufferManagerProxy(
      chromeos::arc::ProtectedBufferManager* protected_buffer_manager);

  // arc::mojom::ProtectedBufferManager implementation.
  void GetProtectedSharedMemoryFromHandle(
      mojo::ScopedHandle dummy_handle,
      GetProtectedSharedMemoryFromHandleCallback callback) override;

 private:
  base::ScopedFD UnwrapFdFromMojoHandle(mojo::ScopedHandle handle);
  mojo::ScopedHandle WrapFdInMojoHandle(base::ScopedFD fd);

  chromeos::arc::ProtectedBufferManager* protected_buffer_manager_;

  DISALLOW_COPY_AND_ASSIGN(GpuArcProtectedBufferManagerProxy);
};

}  // namespace arc
}  // namespace chromeos

#endif  // CHROME_GPU_PROTECTED_BUFFER_MANAGER_PROXY_H_
