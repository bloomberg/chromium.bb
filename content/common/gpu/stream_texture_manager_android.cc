// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/stream_texture_manager_android.h"

#include "content/common/android/surface_texture_bridge.h"
#include "content/common/gpu/gpu_channel.h"

namespace content {

StreamTextureManagerAndroid::StreamTextureAndroid::StreamTextureAndroid(
    GpuChannel* channel, int service_id)
    : surface_texture_(new SurfaceTextureBridge(service_id)),
      channel_(channel) {
  NOTIMPLEMENTED();
}

StreamTextureManagerAndroid::StreamTextureAndroid::~StreamTextureAndroid() {
  NOTIMPLEMENTED();
}

void StreamTextureManagerAndroid::StreamTextureAndroid::Update() {
  surface_texture_->UpdateTexImage();
}

StreamTextureManagerAndroid::StreamTextureManagerAndroid(
    GpuChannel* channel)
    : channel_(channel) {
  NOTIMPLEMENTED();
}

StreamTextureManagerAndroid::~StreamTextureManagerAndroid() {
  NOTIMPLEMENTED();
}

GLuint StreamTextureManagerAndroid::CreateStreamTexture(uint32 service_id,
                                                        uint32 client_id) {
  // service_id: the actual GL texture name
  // client_id: texture name given to the client in the renderer (unused here)
  // The return value here is what glCreateStreamTextureCHROMIUM() will return
  // to identify the stream (i.e. surface texture).
  StreamTextureAndroid* texture = new StreamTextureAndroid(
      channel_, service_id);
  return textures_.Add(texture);
}

void StreamTextureManagerAndroid::DestroyStreamTexture(uint32 service_id) {
  NOTIMPLEMENTED();
}

gpu::StreamTexture* StreamTextureManagerAndroid::LookupStreamTexture(
    uint32 service_id) {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace content
