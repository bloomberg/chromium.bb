// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/transport_texture.h"

TransportTexture::TransportTexture(GpuChannel* channel,
                                   IPC::Message::Sender* sender,
                                   gpu::gles2::GLES2Decoder* decoder,
                                   int32 host_id,
                                   int32 route_id)
  : channel_(channel),
    sender_(sender),
    decoder_(decoder),
    host_id_(host_id),
    route_id_(route_id),
    output_textures_(NULL) {
}

TransportTexture::~TransportTexture() {
}

void TransportTexture::CreateTextures(
    int n, int width, int height, Format format, std::vector<int>* textures,
    Task* done_task) {
  output_textures_ = textures;
  DCHECK(!create_task_.get());
  create_task_.reset(done_task);

  bool ret = sender_->Send(new GpuTransportTextureHostMsg_CreateTextures(
      host_id_, n, width, height, static_cast<int>(format)));
  if (!ret) {
    LOG(ERROR) << "GpuTransportTexture_CreateTextures failed";
  }
}

void TransportTexture::ReleaseTextures() {
  texture_map_.clear();

  bool ret = sender_->Send(new GpuTransportTextureHostMsg_ReleaseTextures(
      host_id_));
  if (!ret) {
    LOG(ERROR) << "GpuTransportTexture_ReleaseTextures failed";
  }
}

void TransportTexture::TextureUpdated(int texture_id) {
  TextureMap::iterator iter = texture_map_.find(texture_id);
  if (iter == texture_map_.end()) {
    LOG(ERROR) << "Texture not found: " << texture_id;
    return;
  }

  bool ret = sender_->Send(new GpuTransportTextureHostMsg_TextureUpdated(
      host_id_, iter->second));
  if (!ret) {
    LOG(ERROR) << "GpuTransportTexture_TextureUpdated failed";
  }
}

void TransportTexture::OnChannelConnected(int32 peer_pid) {
}

void TransportTexture::OnChannelError() {
}

bool TransportTexture::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TransportTexture, msg)
    IPC_MESSAGE_HANDLER(GpuTransportTextureMsg_Destroy,
                        OnDestroy)
    IPC_MESSAGE_HANDLER(GpuTransportTextureMsg_TexturesCreated,
                        OnTexturesCreated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void TransportTexture::OnDestroy() {
  channel_->DestroyTransportTexture(route_id_);
}

void TransportTexture::OnTexturesCreated(const std::vector<int>& textures) {
  bool ret = decoder_->MakeCurrent();
  if (!ret) {
    LOG(ERROR) << "Failed to switch context";
    return;
  }

  output_textures_->clear();
  for (size_t i = 0; i < textures.size(); ++i) {
    uint32 gl_texture = 0;

    // Translate the client texture id to service texture id.
    ret = decoder_->GetServiceTextureId(textures[i], &gl_texture);
    DCHECK(ret) << "Cannot translate client texture ID to service ID";
    output_textures_->push_back(gl_texture);
    texture_map_.insert(std::make_pair(gl_texture, textures[i]));
  }

  // Notify user that textures are ready.
  create_task_->Run();
  create_task_.reset();
  output_textures_ = NULL;
}
