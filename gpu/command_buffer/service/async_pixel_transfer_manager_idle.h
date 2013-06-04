// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_IDLE_H_
#define GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_IDLE_H_

#include "gpu/command_buffer/service/async_pixel_transfer_manager.h"

namespace gpu {
class AsyncPixelTransferDelegateIdle;

class AsyncPixelTransferManagerIdle : public AsyncPixelTransferManager {
 public:
  AsyncPixelTransferManagerIdle();
  virtual ~AsyncPixelTransferManagerIdle();

  // AsyncPixelTransferManager implementation:
  virtual AsyncPixelTransferDelegate* GetAsyncPixelTransferDelegate() OVERRIDE;

 private:
  scoped_ptr<AsyncPixelTransferDelegateIdle> delegate_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferManagerIdle);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_IDLE_H_
