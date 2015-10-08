// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SYNC_TOKEN_H_
#define GPU_COMMAND_BUFFER_COMMON_SYNC_TOKEN_H_

#include <stdint.h>

#include "gpu/command_buffer/common/constants.h"
#include "gpu/gpu_export.h"

// From glextchromium.h.
#ifndef GL_SYNC_TOKEN_SIZE_CHROMIUM
#define GL_SYNC_TOKEN_SIZE_CHROMIUM 24
#endif

namespace gpu {

// A Sync Token is a binary blob which represents a waitable fence sync
// on a particular command buffer namespace and id.
// See src/gpu/GLES2/extensions/CHROMIUM/CHROMIUM_sync_point.txt for more
// details.
struct GPU_EXPORT SyncToken {
  using Data = int8_t[GL_SYNC_TOKEN_SIZE_CHROMIUM];

  SyncToken();
  SyncToken(CommandBufferNamespace namespace_id,
            uint64_t command_buffer_id,
            uint64_t release_count);

  CommandBufferNamespace GetNamespaceId() const;
  uint64_t GetCommandBufferId() const;
  uint64_t GetReleaseCount() const;

  void SetData(CommandBufferNamespace namespace_id,
               uint64_t command_buffer_id,
               uint64_t release_count);

  bool operator<(const SyncToken& other) const;

  Data data;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SYNC_TOKEN_H_

