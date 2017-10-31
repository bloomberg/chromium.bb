// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_DISCARDABLE_TEXTURE_MANAGER_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_DISCARDABLE_TEXTURE_MANAGER_H_

#include <map>

#include "gpu/command_buffer/client/client_discardable_manager.h"
#include "gpu/gpu_export.h"

namespace gpu {

// A helper class used to manage discardable textures. Makes use of
// ClientDiscardableManager. Used by the GLES2 Implementation.
class GPU_EXPORT ClientDiscardableTextureManager {
 public:
  ClientDiscardableTextureManager();
  ~ClientDiscardableTextureManager();
  ClientDiscardableHandle InitializeTexture(CommandBuffer* command_buffer,
                                            uint32_t texture_id);
  bool LockTexture(uint32_t texture_id);
  // Must be called by the GLES2Implementation when a texture is being deleted
  // to allow tracking memory to be reclaimed.
  void FreeTexture(uint32_t texture_id);
  bool TextureIsValid(uint32_t texture_id) const;

  // Test only functions.
  ClientDiscardableManager* DiscardableManagerForTesting() {
    return &discardable_manager_;
  }
  ClientDiscardableHandle GetHandleForTesting(uint32_t texture_id);

 private:
  std::map<uint32_t, ClientDiscardableHandle::Id> texture_id_to_handle_id_;
  ClientDiscardableManager discardable_manager_;

  DISALLOW_COPY_AND_ASSIGN(ClientDiscardableTextureManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_DISCARDABLE_TEXTURE_MANAGER_H_
