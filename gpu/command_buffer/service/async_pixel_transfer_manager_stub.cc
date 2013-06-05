// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_manager_stub.h"

#include "gpu/command_buffer/service/async_pixel_transfer_delegate.h"

namespace gpu {

namespace {

class AsyncPixelTransferStateImpl : public AsyncPixelTransferState {
 public:
  AsyncPixelTransferStateImpl() {}

  // Implement AsyncPixelTransferState:
  virtual bool TransferIsInProgress() OVERRIDE {
    return false;
  }

 private:
  virtual ~AsyncPixelTransferStateImpl() {}
};

}  // namespace

class AsyncPixelTransferDelegateStub : public AsyncPixelTransferDelegate {
 public:
  AsyncPixelTransferDelegateStub();
  virtual ~AsyncPixelTransferDelegateStub();

  // Implement AsyncPixelTransferDelegate:
  virtual AsyncPixelTransferState* CreatePixelTransferState(
      GLuint texture_id,
      const AsyncTexImage2DParams& define_params) OVERRIDE;
  virtual void AsyncTexImage2D(
      AsyncPixelTransferState* state,
      const AsyncTexImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params,
      const base::Closure& bind_callback) OVERRIDE;
  virtual void AsyncTexSubImage2D(
      AsyncPixelTransferState* transfer_state,
      const AsyncTexSubImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params) OVERRIDE;
  virtual void WaitForTransferCompletion(
      AsyncPixelTransferState* state) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferDelegateStub);
};

AsyncPixelTransferDelegateStub::AsyncPixelTransferDelegateStub() {}

AsyncPixelTransferDelegateStub::~AsyncPixelTransferDelegateStub() {}

AsyncPixelTransferState* AsyncPixelTransferDelegateStub::
    CreatePixelTransferState(GLuint texture_id,
                             const AsyncTexImage2DParams& define_params) {
  return new AsyncPixelTransferStateImpl;
}

void AsyncPixelTransferDelegateStub::AsyncTexImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params,
    const base::Closure& bind_callback) {
  bind_callback.Run();
}

void AsyncPixelTransferDelegateStub::AsyncTexSubImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexSubImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
}

void AsyncPixelTransferDelegateStub::WaitForTransferCompletion(
    AsyncPixelTransferState* state) {
}

AsyncPixelTransferManagerStub::AsyncPixelTransferManagerStub()
    : delegate_(new AsyncPixelTransferDelegateStub()) {}

AsyncPixelTransferManagerStub::~AsyncPixelTransferManagerStub() {}

void AsyncPixelTransferManagerStub::BindCompletedAsyncTransfers() {
}

void AsyncPixelTransferManagerStub::AsyncNotifyCompletion(
    const AsyncMemoryParams& mem_params,
    const CompletionCallback& callback) {
  callback.Run(mem_params);
}

uint32 AsyncPixelTransferManagerStub::GetTextureUploadCount() {
  return 0;
}

base::TimeDelta AsyncPixelTransferManagerStub::GetTotalTextureUploadTime() {
  return base::TimeDelta();
}

void AsyncPixelTransferManagerStub::ProcessMorePendingTransfers() {
}

bool AsyncPixelTransferManagerStub::NeedsProcessMorePendingTransfers() {
  return false;
}

AsyncPixelTransferDelegate*
AsyncPixelTransferManagerStub::GetAsyncPixelTransferDelegate() {
  return delegate_.get();
}

}  // namespace gpu
