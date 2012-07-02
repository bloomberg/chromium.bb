// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_STREAM_TEXTURE_MANAGER_ANDROID_H_
#define CONTENT_COMMON_GPU_STREAM_TEXTURE_MANAGER_ANDROID_H_
#pragma once

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/service/stream_texture.h"
#include "gpu/command_buffer/service/stream_texture_manager.h"

class GpuChannel;

namespace content {

class SurfaceTextureBridge;

// Class for managing the stream texture.
class StreamTextureManagerAndroid : public gpu::StreamTextureManager {
 public:
  StreamTextureManagerAndroid(GpuChannel* channel);
  virtual ~StreamTextureManagerAndroid();

  // implement gpu::StreamTextureManager:
  GLuint CreateStreamTexture(uint32 service_id,
                             uint32 client_id) OVERRIDE;
  void DestroyStreamTexture(uint32 service_id) OVERRIDE;
  gpu::StreamTexture* LookupStreamTexture(uint32 service_id) OVERRIDE;

 private:
  // The stream texture class for android.
  class StreamTextureAndroid
      : public gpu::StreamTexture,
        public base::SupportsWeakPtr<StreamTextureAndroid> {
   public:
    StreamTextureAndroid(GpuChannel* channel, int service_id);
    virtual ~StreamTextureAndroid();

    void Update() OVERRIDE;

    SurfaceTextureBridge* bridge() { return surface_texture_.get(); }

   private:
    scoped_ptr<SurfaceTextureBridge> surface_texture_;
    GpuChannel* channel_;

    DISALLOW_COPY_AND_ASSIGN(StreamTextureAndroid);
  };

  GpuChannel* channel_;

  typedef IDMap<StreamTextureAndroid, IDMapOwnPointer> TextureMap;
  TextureMap textures_;

  DISALLOW_COPY_AND_ASSIGN(StreamTextureManagerAndroid);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_STREAM_TEXTURE_MANAGER_ANDROID_H_
