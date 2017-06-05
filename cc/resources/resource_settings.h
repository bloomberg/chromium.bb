// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_SETTINGS_H_
#define CC_RESOURCES_RESOURCE_SETTINGS_H_

#include "cc/cc_export.h"
#include "cc/output/buffer_to_texture_target_map.h"

namespace cc {

// ResourceSettings contains all the settings that are needed to create a
// ResourceProvider.
class CC_EXPORT ResourceSettings {
 public:
  ResourceSettings();
  ResourceSettings(const ResourceSettings& other);
  ResourceSettings& operator=(const ResourceSettings& other);
  ~ResourceSettings();

  size_t texture_id_allocation_chunk_size = 64;
  bool use_gpu_memory_buffer_resources = false;
  BufferToTextureTargetMap buffer_to_texture_target_map;
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_SETTINGS_H_
