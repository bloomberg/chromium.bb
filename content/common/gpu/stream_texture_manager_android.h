// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_STREAM_TEXTURE_MANAGER_ANDROID_H_
#define CONTENT_COMMON_GPU_STREAM_TEXTURE_MANAGER_ANDROID_H_

#include "base/id_map.h"
#include "gpu/command_buffer/service/stream_texture.h"
#include "gpu/command_buffer/service/stream_texture_manager.h"

struct GpuStreamTextureMsg_MatrixChanged_Params;

namespace gfx {
class Size;
class SurfaceTexture;
}

namespace content {
class GpuChannel;

// Class for managing the stream texture.
class StreamTextureManagerAndroid : public gpu::StreamTextureManager {
 public:
  StreamTextureManagerAndroid(GpuChannel* channel);
  virtual ~StreamTextureManagerAndroid();

  // implement gpu::StreamTextureManager:
  virtual uint32 CreateStreamTexture(uint32 service_id,
                                     uint32 client_id) OVERRIDE;
  virtual void DestroyStreamTexture(uint32 service_id) OVERRIDE;
  virtual gpu::StreamTexture* LookupStreamTexture(uint32 service_id) OVERRIDE;

 private:
  GpuChannel* channel_;

  typedef IDMap<gpu::StreamTexture, IDMapOwnPointer> TextureMap;
  TextureMap textures_;

  // Map for more convenient lookup.
  typedef IDMap<gpu::StreamTexture, IDMapExternalPointer> TextureServiceIdMap;
  TextureServiceIdMap textures_from_service_id_;

  DISALLOW_COPY_AND_ASSIGN(StreamTextureManagerAndroid);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_STREAM_TEXTURE_MANAGER_ANDROID_H_
