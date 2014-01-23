// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/stream_texture_manager_android.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_messages.h"
#include "gpu/command_buffer/service/stream_texture.h"
#include "ipc/ipc_listener.h"
#include "ui/gfx/size.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

namespace content {

namespace {

class StreamTextureImpl : public gpu::StreamTexture,
                          public IPC::Listener {
 public:
  StreamTextureImpl(GpuChannel* channel, int service_id, int32 route_id);
  virtual ~StreamTextureImpl();

  // gpu::StreamTexture implementation:
  virtual void Update() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Called when a new frame is available for the SurfaceTexture.
  void OnFrameAvailable();

  // IPC message handlers:
  void OnStartListening();
  void OnEstablishPeer(int32 primary_id, int32 secondary_id);
  void OnSetSize(const gfx::Size& size) { size_ = size; }

  scoped_refptr<gfx::SurfaceTexture> surface_texture_;

  // Current transform matrix of the surface texture.
  float current_matrix_[16];

  // Current size of the surface texture.
  gfx::Size size_;

  // Whether the surface texture has been updated.
  bool has_updated_;

  GpuChannel* channel_;
  int32 route_id_;
  bool has_listener_;

  base::WeakPtrFactory<StreamTextureImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(StreamTextureImpl);
};

StreamTextureImpl::StreamTextureImpl(GpuChannel* channel,
                                     int service_id,
                                     int32 route_id)
    : surface_texture_(new gfx::SurfaceTexture(service_id)),
      size_(0, 0),
      has_updated_(false),
      channel_(channel),
      route_id_(route_id),
      has_listener_(false),
      weak_factory_(this) {
  memset(current_matrix_, 0, sizeof(current_matrix_));
  channel_->AddRoute(route_id, this);
}

StreamTextureImpl::~StreamTextureImpl() {
  channel_->RemoveRoute(route_id_);
}

void StreamTextureImpl::Update() {
  surface_texture_->UpdateTexImage();
  if (!has_listener_)
    return;

  float mtx[16];
  surface_texture_->GetTransformMatrix(mtx);

  // Only query the matrix once we have bound a valid frame.
  if (has_updated_ && memcmp(current_matrix_, mtx, sizeof(mtx)) != 0) {
    memcpy(current_matrix_, mtx, sizeof(mtx));

    GpuStreamTextureMsg_MatrixChanged_Params params;
    memcpy(&params.m00, mtx, sizeof(mtx));
    channel_->Send(new GpuStreamTextureMsg_MatrixChanged(route_id_, params));
  }
}

void StreamTextureImpl::OnFrameAvailable() {
  has_updated_ = true;
  DCHECK(has_listener_);
  channel_->Send(new GpuStreamTextureMsg_FrameAvailable(route_id_));
}

gfx::Size StreamTextureImpl::GetSize() {
  return size_;
}

bool StreamTextureImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(StreamTextureImpl, message)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_StartListening, OnStartListening)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_EstablishPeer, OnEstablishPeer)
    IPC_MESSAGE_HANDLER(GpuStreamTextureMsg_SetSize, OnSetSize)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  DCHECK(handled);
  return handled;
}

void StreamTextureImpl::OnStartListening() {
  DCHECK(!has_listener_);
  has_listener_ = true;
  surface_texture_->SetFrameAvailableCallback(base::Bind(
      &StreamTextureImpl::OnFrameAvailable, weak_factory_.GetWeakPtr()));
}

void StreamTextureImpl::OnEstablishPeer(int32 primary_id, int32 secondary_id) {
  base::ProcessHandle process = channel_->renderer_pid();

  SurfaceTexturePeer::GetInstance()->EstablishSurfaceTexturePeer(
      process, surface_texture_, primary_id, secondary_id);
}

}  // anonymous namespace

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
  int32 route_id = channel_->GenerateRouteID();
  StreamTextureImpl* texture =
      new StreamTextureImpl(channel_, service_id, route_id);
  textures_from_service_id_.AddWithID(texture, service_id);
  textures_.AddWithID(texture, route_id);
  return route_id;
}

void StreamTextureManagerAndroid::DestroyStreamTexture(uint32 service_id) {
  gpu::StreamTexture* texture = textures_from_service_id_.Lookup(service_id);
  if (texture) {
    textures_from_service_id_.Remove(service_id);

    for (TextureMap::Iterator<gpu::StreamTexture> it(&textures_);
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

}  // namespace content
