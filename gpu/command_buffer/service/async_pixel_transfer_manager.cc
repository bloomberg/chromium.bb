// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_manager.h"

#include "gpu/command_buffer/service/async_pixel_transfer_delegate.h"

namespace gpu {

AsyncPixelTransferManager::AsyncPixelTransferManager(
    gles2::TextureManager* manager,
    gfx::GLContext* context)
    : manager_(manager),
      delegate_(AsyncPixelTransferDelegate::Create(context)) {
  manager_->AddObserver(this);
}

AsyncPixelTransferManager::~AsyncPixelTransferManager() {
  if (manager_)
    manager_->RemoveObserver(this);
}

AsyncPixelTransferState* AsyncPixelTransferManager::CreatePixelTransferState(
    gles2::TextureRef* ref,
    const AsyncTexImage2DParams& define_params) {
  DCHECK(!GetPixelTransferState(ref));
  AsyncPixelTransferState* state = delegate_->CreatePixelTransferState(
      ref->texture()->service_id(), define_params);
  state_map_[ref] = state;
  return state;
}

AsyncPixelTransferState*
AsyncPixelTransferManager::GetPixelTransferState(
    gles2::TextureRef* ref) {
  TextureToStateMap::iterator it = state_map_.find(ref);
  if (it == state_map_.end()) {
    return NULL;
  } else {
    return it->second;
  }
}

void AsyncPixelTransferManager::ClearPixelTransferStateForTest(
    gles2::TextureRef* ref) {
  TextureToStateMap::iterator it = state_map_.find(ref);
  if (it != state_map_.end())
    state_map_.erase(it);
}

bool AsyncPixelTransferManager::AsyncTransferIsInProgress(
    gles2::TextureRef* ref) {
  AsyncPixelTransferState* state = GetPixelTransferState(ref);
  return state && state->TransferIsInProgress();
}

void AsyncPixelTransferManager::OnTextureManagerDestroying(
    gles2::TextureManager* manager) {
  // TextureManager should outlive AsyncPixelTransferManager.
  NOTREACHED();
  manager_ = NULL;
}

void AsyncPixelTransferManager::OnTextureRefDestroying(
    gles2::TextureRef* texture) {
  TextureToStateMap::iterator it = state_map_.find(texture);
  if (it != state_map_.end())
    state_map_.erase(it);
}

}  // namespace gpu
