// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_DELEGATE_MOCK
#define GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_DELEGATE_MOCK

#include "base/basictypes.h"
#include "gpu/command_buffer/service/async_pixel_transfer_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

class MockAsyncPixelTransferState : public AsyncPixelTransferState {
 public:
  MockAsyncPixelTransferState();

  // Called in ~MockAsyncPixelTransferState.
  MOCK_METHOD0(Destroy, void());

  // Implement AsyncPixelTransferState.
  MOCK_METHOD0(TransferIsInProgress, bool());
  MOCK_METHOD1(BindTransfer, void(AsyncTexImage2DParams* level_params));

 protected:
  virtual ~MockAsyncPixelTransferState();
  DISALLOW_COPY_AND_ASSIGN(MockAsyncPixelTransferState);
};

class MockAsyncPixelTransferDelegate : public AsyncPixelTransferDelegate {
 public:
  MockAsyncPixelTransferDelegate();
  virtual ~MockAsyncPixelTransferDelegate();

  // Implement AsyncPixelTransferDelegate.
  MOCK_METHOD2(CreatePixelTransferState,
      AsyncPixelTransferState*(
          GLuint service_id, const AsyncTexImage2DParams& define_params));
  MOCK_METHOD4(AsyncTexImage2D,
      void(AsyncPixelTransferState*,
          const AsyncTexImage2DParams& tex_params,
          const AsyncMemoryParams& mem_params,
          const base::Closure& bind_callback));
  MOCK_METHOD3(AsyncTexSubImage2D,
      void(AsyncPixelTransferState*,
          const AsyncTexSubImage2DParams& tex_params,
          const AsyncMemoryParams& mem_params));
  MOCK_METHOD1(WaitForTransferCompletion, void(AsyncPixelTransferState*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAsyncPixelTransferDelegate);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ASYNC_PIXEL_TRANSFER_DELEGATE_MOCK
