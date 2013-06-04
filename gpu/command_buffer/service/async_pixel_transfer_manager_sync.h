// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_SYNC_H_
#define GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_SYNC_H_

#include "gpu/command_buffer/service/async_pixel_transfer_manager.h"

namespace gpu {
class AsyncPixelTransferDelegateSync;

class AsyncPixelTransferManagerSync : public AsyncPixelTransferManager {
 public:
  AsyncPixelTransferManagerSync();
  virtual ~AsyncPixelTransferManagerSync();

  // AsyncPixelTransferManager implementation:
  virtual AsyncPixelTransferDelegate* GetAsyncPixelTransferDelegate() OVERRIDE;

 private:
  scoped_ptr<AsyncPixelTransferDelegateSync> delegate_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferManagerSync);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_SYNC_H_
