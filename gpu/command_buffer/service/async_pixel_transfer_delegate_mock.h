// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ASYNC_TASK_DELEGATE_MOCK_H_
#define UI_GL_ASYNC_TASK_DELEGATE_MOCK_H_

#include "base/basictypes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gl/async_pixel_transfer_delegate.h"

namespace gfx {

class MockAsyncPixelTransferState : public gfx::AsyncPixelTransferState {
 public:
  MockAsyncPixelTransferState();

  // Implement AsyncPixelTransferState.
  MOCK_METHOD0(TransferIsInProgress, bool());
  MOCK_METHOD1(BindTransfer, void(AsyncTexImage2DParams* level_params));

 protected:
  virtual ~MockAsyncPixelTransferState();
  DISALLOW_COPY_AND_ASSIGN(MockAsyncPixelTransferState);
};

class MockAsyncPixelTransferDelegate : public gfx::AsyncPixelTransferDelegate {
 public:
  MockAsyncPixelTransferDelegate();
  virtual ~MockAsyncPixelTransferDelegate();

  // Implement AsyncPixelTransferDelegate.
  MOCK_METHOD1(CreateRawPixelTransferState,
      gfx::AsyncPixelTransferState*(GLuint service_id));
  MOCK_METHOD2(AsyncNotifyCompletion,
      void(const AsyncMemoryParams& mem_params,
           const CompletionCallback& callback));
  MOCK_METHOD3(AsyncTexImage2D,
      void(gfx::AsyncPixelTransferState*,
          const AsyncTexImage2DParams& tex_params,
          const AsyncMemoryParams& mem_params));
  MOCK_METHOD3(AsyncTexSubImage2D,
      void(gfx::AsyncPixelTransferState*,
          const AsyncTexSubImage2DParams& tex_params,
          const AsyncMemoryParams& mem_params));
  MOCK_METHOD1(WaitForTransferCompletion, void(gfx::AsyncPixelTransferState*));
  MOCK_METHOD0(GetTextureUploadCount, uint32());
  MOCK_METHOD0(GetTotalTextureUploadTime, base::TimeDelta());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAsyncPixelTransferDelegate);
};

}  // namespace gfx

#endif  // UI_GL_ASYNC_TASK_DELEGATE_MOCK_H_

