// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_manager.h"

#include "gpu/command_buffer/service/async_pixel_transfer_delegate.h"

namespace gpu {

AsyncPixelTransferCompletionObserver::AsyncPixelTransferCompletionObserver() {}

AsyncPixelTransferCompletionObserver::~AsyncPixelTransferCompletionObserver() {}

AsyncPixelTransferManager::AsyncPixelTransferManager() {}

AsyncPixelTransferManager::~AsyncPixelTransferManager() {
  if (manager_)
    manager_->RemoveObserver(this);
}

void AsyncPixelTransferManager::Initialize(gles2::TextureManager* manager) {
  manager_ = manager;
  manager_->AddObserver(this);
}

AsyncPixelTransferDelegate*
AsyncPixelTransferManager::CreatePixelTransferDelegate(
    gles2::TextureRef* ref,
    const AsyncTexImage2DParams& define_params) {
  DCHECK(!GetPixelTransferDelegate(ref));
  AsyncPixelTransferDelegate* delegate =
      CreatePixelTransferDelegateImpl(ref, define_params);
  delegate_map_[ref] = make_linked_ptr(delegate);
  return delegate;
}

AsyncPixelTransferDelegate*
AsyncPixelTransferManager::GetPixelTransferDelegate(
    gles2::TextureRef* ref) {
  TextureToDelegateMap::iterator it = delegate_map_.find(ref);
  if (it == delegate_map_.end()) {
    return NULL;
  } else {
    return it->second.get();
  }
}

void AsyncPixelTransferManager::ClearPixelTransferDelegateForTest(
    gles2::TextureRef* ref) {
  TextureToDelegateMap::iterator it = delegate_map_.find(ref);
  if (it != delegate_map_.end())
    delegate_map_.erase(it);
}

bool AsyncPixelTransferManager::AsyncTransferIsInProgress(
    gles2::TextureRef* ref) {
  AsyncPixelTransferDelegate* delegate = GetPixelTransferDelegate(ref);
  return delegate && delegate->TransferIsInProgress();
}

void AsyncPixelTransferManager::OnTextureManagerDestroying(
    gles2::TextureManager* manager) {
  // TextureManager should outlive AsyncPixelTransferManager.
  NOTREACHED();
  manager_ = NULL;
}

void AsyncPixelTransferManager::OnTextureRefDestroying(
    gles2::TextureRef* texture) {
  TextureToDelegateMap::iterator it = delegate_map_.find(texture);
  if (it != delegate_map_.end())
    delegate_map_.erase(it);
}

}  // namespace gpu
