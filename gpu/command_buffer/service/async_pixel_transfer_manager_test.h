// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_TEST_H_
#define GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_TEST_H_

#include "gpu/command_buffer/service/async_pixel_transfer_manager.h"

#include "gpu/command_buffer/service/async_pixel_transfer_delegate_mock.h"

namespace gpu {

class AsyncPixelTransferManagerTest : public AsyncPixelTransferManager {
 public:
  AsyncPixelTransferManagerTest();
  virtual ~AsyncPixelTransferManagerTest();

  // AsyncPixelTransferManager implementation:
  virtual AsyncPixelTransferDelegate* GetAsyncPixelTransferDelegate() OVERRIDE;

  ::testing::StrictMock<MockAsyncPixelTransferDelegate>* GetMockDelegate() {
    return delegate_.get();
  }

 private:
  scoped_ptr< ::testing::StrictMock<MockAsyncPixelTransferDelegate> > delegate_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferManagerTest);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_MANAGER_TEST_H_
