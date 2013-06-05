// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_manager_sync.h"

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

// Class which handles async pixel transfers synchronously.
class AsyncPixelTransferDelegateSync : public AsyncPixelTransferDelegate {
 public:
  AsyncPixelTransferDelegateSync();
  virtual ~AsyncPixelTransferDelegateSync();

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
  uint32 GetTextureUploadCount();
  base::TimeDelta GetTotalTextureUploadTime();

 private:
  int texture_upload_count_;
  base::TimeDelta total_texture_upload_time_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferDelegateSync);
};

AsyncPixelTransferDelegateSync::AsyncPixelTransferDelegateSync()
    : texture_upload_count_(0) {
}

AsyncPixelTransferDelegateSync::~AsyncPixelTransferDelegateSync() {}

AsyncPixelTransferState* AsyncPixelTransferDelegateSync::
    CreatePixelTransferState(GLuint texture_id,
                             const AsyncTexImage2DParams& define_params) {
  return new AsyncPixelTransferStateImpl;
}

void AsyncPixelTransferDelegateSync::AsyncTexImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params,
    const base::Closure& bind_callback) {
  // Save the define params to return later during deferred
  // binding of the transfer texture.
  DCHECK(transfer_state);
  void* data = GetAddress(mem_params);
  glTexImage2D(
      tex_params.target,
      tex_params.level,
      tex_params.internal_format,
      tex_params.width,
      tex_params.height,
      tex_params.border,
      tex_params.format,
      tex_params.type,
      data);
  // The texture is already fully bound so just call it now.
  bind_callback.Run();
}

void AsyncPixelTransferDelegateSync::AsyncTexSubImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexSubImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
  void* data = GetAddress(mem_params);
  DCHECK(transfer_state);
  base::TimeTicks begin_time(base::TimeTicks::HighResNow());
  glTexSubImage2D(
      tex_params.target,
      tex_params.level,
      tex_params.xoffset,
      tex_params.yoffset,
      tex_params.width,
      tex_params.height,
      tex_params.format,
      tex_params.type,
      data);
  texture_upload_count_++;
  total_texture_upload_time_ += base::TimeTicks::HighResNow() - begin_time;
}

void AsyncPixelTransferDelegateSync::WaitForTransferCompletion(
    AsyncPixelTransferState* state) {
  // Already done.
}

uint32 AsyncPixelTransferDelegateSync::GetTextureUploadCount() {
  return texture_upload_count_;
}

base::TimeDelta AsyncPixelTransferDelegateSync::GetTotalTextureUploadTime() {
  return total_texture_upload_time_;
}

AsyncPixelTransferManagerSync::AsyncPixelTransferManagerSync()
    : delegate_(new AsyncPixelTransferDelegateSync()) {}

AsyncPixelTransferManagerSync::~AsyncPixelTransferManagerSync() {}

void AsyncPixelTransferManagerSync::BindCompletedAsyncTransfers() {
  // Everything is already bound.
}

void AsyncPixelTransferManagerSync::AsyncNotifyCompletion(
    const AsyncMemoryParams& mem_params,
    const CompletionCallback& callback) {
  callback.Run(mem_params);
}

uint32 AsyncPixelTransferManagerSync::GetTextureUploadCount() {
  return delegate_->GetTextureUploadCount();
}

base::TimeDelta AsyncPixelTransferManagerSync::GetTotalTextureUploadTime() {
  return delegate_->GetTotalTextureUploadTime();
}

void AsyncPixelTransferManagerSync::ProcessMorePendingTransfers() {
}

bool AsyncPixelTransferManagerSync::NeedsProcessMorePendingTransfers() {
  return false;
}

AsyncPixelTransferDelegate*
AsyncPixelTransferManagerSync::GetAsyncPixelTransferDelegate() {
  return delegate_.get();
}

}  // namespace gpu

