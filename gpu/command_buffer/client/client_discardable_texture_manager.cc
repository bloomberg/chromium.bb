// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/client_discardable_texture_manager.h"

namespace gpu {

ClientDiscardableTextureManager::ClientDiscardableTextureManager() = default;
ClientDiscardableTextureManager::~ClientDiscardableTextureManager() = default;

ClientDiscardableHandle ClientDiscardableTextureManager::InitializeTexture(
    CommandBuffer* command_buffer,
    uint32_t texture_id) {
  base::AutoLock hold(lock_);
  DCHECK(texture_id_to_handle_id_.find(texture_id) ==
         texture_id_to_handle_id_.end());
  ClientDiscardableHandle::Id handle_id =
      discardable_manager_.CreateHandle(command_buffer);
  if (handle_id.is_null())
    return ClientDiscardableHandle();

  texture_id_to_handle_id_[texture_id] = handle_id;
  return discardable_manager_.GetHandle(handle_id);
}

bool ClientDiscardableTextureManager::LockTexture(uint32_t texture_id) {
  base::AutoLock hold(lock_);
  auto found = texture_id_to_handle_id_.find(texture_id);
  DCHECK(found != texture_id_to_handle_id_.end());
  return discardable_manager_.LockHandle(found->second);
}

void ClientDiscardableTextureManager::FreeTexture(uint32_t texture_id) {
  base::AutoLock hold(lock_);
  auto found = texture_id_to_handle_id_.find(texture_id);
  if (found == texture_id_to_handle_id_.end())
    return;
  ClientDiscardableHandle::Id discardable_id = found->second;
  texture_id_to_handle_id_.erase(found);
  return discardable_manager_.FreeHandle(discardable_id);
}

bool ClientDiscardableTextureManager::TextureIsValid(
    uint32_t texture_id) const {
  base::AutoLock hold(lock_);
  return texture_id_to_handle_id_.find(texture_id) !=
         texture_id_to_handle_id_.end();
}

ClientDiscardableHandle ClientDiscardableTextureManager::GetHandleForTesting(
    uint32_t texture_id) {
  base::AutoLock hold(lock_);
  auto found = texture_id_to_handle_id_.find(texture_id);
  DCHECK(found != texture_id_to_handle_id_.end());
  return discardable_manager_.GetHandle(found->second);
}

}  // namespace gpu
