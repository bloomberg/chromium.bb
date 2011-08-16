// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This clasis used together with TansportTextureHost in the renderer process
// to provide a mechanism for an object in the GPU process to create textures
// and be used in the renderer process.
//
// See content/gpu/transport_texture_host.h for usage and details.

#ifndef CONTENT_COMMON_GPU_TRANSPORT_TEXTURE_H_
#define CONTENT_COMMON_GPU_TRANSPORT_TEXTURE_H_

#include <vector>
#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "ipc/ipc_channel.h"

class GpuChannel;

namespace gpu {
namespace gles2 {
class GLES2Decoder;
}  // namespace gles2
}  // namespace gpu

class TransportTexture : public IPC::Channel::Listener {
 public:
  enum Format {
    RGBA,  // GL_RGBA
  };

  // |channel| is the owner of this object.
  // |sender| is used to send send IPC messages.
  // |decoder| is the decoder for GLES2 command buffer, used to convert texture
  // IDs.
  // |host_id| is ID of TransportTextureHost in Renderer process.
  // |id| is ID of this object in GpuChannel.
  TransportTexture(GpuChannel* channel,
                   IPC::Message::Sender* sender,
                   gpu::gles2::GLES2Decoder* decoder,
                   int32 host_id,
                   int32 route_id);
  virtual ~TransportTexture();

  // Create a set of textures of specified size and format. They will be
  // stored in |textures| and |done_task| will be called.
  // Textures IDs stored in |textures| are in the system GL context.
  void CreateTextures(int n, int width, int height, Format format,
                      std::vector<int>* textures, Task* done_task);

  // Release all textures that have previously been allocated.
  void ReleaseTextures();

  // Notify that the texture has been updated. Notification will be sent to the
  // renderer process.
  void TextureUpdated(int texture_id);

  // IPC::Channel::Listener implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  // Mapping from service (GPU) IDs to client (Renderer) IDs.
  typedef std::map<int, int> TextureMap;

  ///////////////////////////////////////////////////////////////////////////
  // IPC Message Handlers
  void OnDestroy();
  void OnTexturesCreated(const std::vector<int>& textures);

  GpuChannel* channel_;
  IPC::Message::Sender* sender_;
  gpu::gles2::GLES2Decoder* decoder_;

  // ID of TransportTextureHost in the Renderer process.
  int32 host_id_;

  // ID of this object in GpuChannel.
  int32 route_id_;

  // Output pointer to write generated textures.
  std::vector<int>* output_textures_;

  // Task that gets called when textures are generated.
  scoped_ptr<Task> create_task_;

  // Mapping between service (GPU) IDs to client (Renderer) IDs.
  TextureMap texture_map_;

  DISALLOW_COPY_AND_ASSIGN(TransportTexture);
};

#endif  // CONTENT_COMMON_GPU_TRANSPORT_TEXTURE_H_
