// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_STREAM_TEXTURE_HOST_ANDROID_H_
#define CONTENT_RENDERER_GPU_STREAM_TEXTURE_HOST_ANDROID_H_

#include "base/memory/weak_ptr.h"
#include "content/common/android/surface_texture_peer.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"

namespace gfx {
class Size;
}

struct GpuStreamTextureMsg_MatrixChanged_Params;

namespace content {
class GpuChannelHost;

// Class for handling all the IPC messages between the GPU process and
// StreamTextureProxy.
class StreamTextureHost : public IPC::Listener {
 public:
  explicit StreamTextureHost(GpuChannelHost* channel);
  virtual ~StreamTextureHost();

  bool Initialize(int stream_id, const gfx::Size& initial_size);

  // Listener class that is listening to the stream texture updates. It is
  // implemented by StreamTextureProxyImpl.
  class Listener {
   public:
    virtual void OnFrameAvailable() = 0;
    virtual void OnMatrixChanged(const float mtx[16]) = 0;
    virtual ~Listener() {}
  };

  void SetListener(Listener* listener) { listener_ = listener; }

  // Request the GPU process to create the surface texture and forward it
  // to the renderer process.
  void EstablishPeer(int32 primary_id, int32 secondary_id);

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  // Message handlers:
  void OnFrameAvailable();
  void OnMatrixChanged(const GpuStreamTextureMsg_MatrixChanged_Params& param);

  int route_id_;
  int stream_id_;
  Listener* listener_;
  scoped_refptr<GpuChannelHost> channel_;
  base::WeakPtrFactory<StreamTextureHost> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamTextureHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_STREAM_TEXTURE_HOST_ANDROID_H_
