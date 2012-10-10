// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/stream_texture_manager_android.h"

#include "base/bind.h"
#include "content/common/android/surface_texture_bridge.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/service/stream_texture.h"
#include "ui/gfx/size.h"

namespace content {

StreamTextureManagerAndroid::StreamTextureAndroid::StreamTextureAndroid(
    GpuChannel* channel, int service_id)
    : surface_texture_bridge_(new SurfaceTextureBridge(service_id)),
      has_updated_(false),
      channel_(channel) {
  memset(current_matrix_, 0, sizeof(current_matrix_));
}

StreamTextureManagerAndroid::StreamTextureAndroid::~StreamTextureAndroid() {
}

void StreamTextureManagerAndroid::StreamTextureAndroid::Update() {
  surface_texture_bridge_->UpdateTexImage();
  if (matrix_callback_.is_null())
    return;

  float mtx[16];
  surface_texture_bridge_->GetTransformMatrix(mtx);

  // Only query the matrix once we have bound a valid frame.
  if (has_updated_ && memcmp(current_matrix_, mtx, sizeof(mtx)) != 0) {
    memcpy(current_matrix_, mtx, sizeof(mtx));

    GpuStreamTextureMsg_MatrixChanged_Params params;
    memcpy(&params.m00, mtx, sizeof(mtx));
    matrix_callback_.Run(params);
  }
}

void StreamTextureManagerAndroid::StreamTextureAndroid::OnFrameAvailable(
    int route_id) {
  has_updated_ = true;
  channel_->Send(new GpuStreamTextureMsg_FrameAvailable(route_id));
}

StreamTextureManagerAndroid::StreamTextureManagerAndroid(
    GpuChannel* channel)
    : channel_(channel) {
}

StreamTextureManagerAndroid::~StreamTextureManagerAndroid() {
  DCHECK(textures_.size() == textures_from_service_id_.size());
  if (!textures_.IsEmpty())
    LOG(WARNING) << "Undestroyed surface textures while closing GPU channel.";
}

GLuint StreamTextureManagerAndroid::CreateStreamTexture(uint32 service_id,
                                                        uint32 client_id) {
  // service_id: the actual GL texture name
  // client_id: texture name given to the client in the renderer (unused here)
  // The return value here is what glCreateStreamTextureCHROMIUM() will return
  // to identify the stream (i.e. surface texture).
  StreamTextureAndroid* texture = new StreamTextureAndroid(
      channel_, service_id);
  textures_from_service_id_.AddWithID(texture, service_id);
  return textures_.Add(texture);
}

void StreamTextureManagerAndroid::DestroyStreamTexture(uint32 service_id) {
  gpu::StreamTexture* texture = textures_from_service_id_.Lookup(service_id);
  if (texture) {
    textures_from_service_id_.Remove(service_id);

    for (TextureMap::Iterator<StreamTextureAndroid> it(&textures_);
        !it.IsAtEnd(); it.Advance()) {
      if (it.GetCurrentValue() == texture) {
        textures_.Remove(it.GetCurrentKey());
        break;
      }
    }
  }
}

gpu::StreamTexture* StreamTextureManagerAndroid::LookupStreamTexture(
    uint32 service_id) {
  return textures_from_service_id_.Lookup(service_id);
}

void StreamTextureManagerAndroid::SendMatrixChanged(
    int route_id,
    const GpuStreamTextureMsg_MatrixChanged_Params& params) {
  channel_->Send(new GpuStreamTextureMsg_MatrixChanged(route_id, params));
}

void StreamTextureManagerAndroid::RegisterStreamTextureProxy(
    int32 stream_id, const gfx::Size& initial_size, int32 route_id) {
  StreamTextureAndroid* stream_texture = textures_.Lookup(stream_id);
  if (stream_texture) {
    // TODO(sievers): Post from binder thread to IO thread directly.
    base::Closure frame_cb = base::Bind(
          &StreamTextureAndroid::OnFrameAvailable,
          stream_texture->AsWeakPtr(),
          route_id);
    StreamTextureAndroid::MatrixChangedCB matrix_cb = base::Bind(
          &StreamTextureManagerAndroid::SendMatrixChanged,
          base::Unretained(this),
          route_id);
    stream_texture->set_matrix_changed_callback(matrix_cb);
    stream_texture->surface_texture_bridge()->SetFrameAvailableCallback(
        frame_cb);
    stream_texture->surface_texture_bridge()->SetDefaultBufferSize(
        initial_size.width(), initial_size.height());
  }
}

void StreamTextureManagerAndroid::EstablishStreamTexture(
    int32 stream_id, SurfaceTexturePeer::SurfaceTextureTarget type,
    int32 primary_id, int32 secondary_id) {
  StreamTextureAndroid* stream_texture = textures_.Lookup(stream_id);
  base::ProcessHandle process = channel_->renderer_pid();

  if (stream_texture) {
    SurfaceTexturePeer::GetInstance()->EstablishSurfaceTexturePeer(
        process,
        type,
        stream_texture->surface_texture_bridge(),
        primary_id,
        secondary_id);
  }
}

}  // namespace content
