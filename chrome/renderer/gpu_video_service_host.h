// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GPU_VIDEO_SERVICE_HOST_H_
#define CHROME_RENDERER_GPU_VIDEO_SERVICE_HOST_H_

#include "base/singleton.h"
#include "chrome/common/gpu_video_common.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "chrome/renderer/gpu_video_decoder_host.h"
#include "ipc/ipc_channel.h"
#include "media/base/buffers.h"
#include "media/base/video_frame.h"

// A GpuVideoServiceHost is a singleton in the renderer process that provides
// access to hardware accelerated video decoding device in the GPU process
// though a GpuVideoDecoderHost.
class GpuVideoServiceHost : public IPC::Channel::Listener,
                            public Singleton<GpuVideoServiceHost> {
 public:
  // IPC::Channel::Listener.
  virtual void OnChannelConnected(int32 peer_pid) {}
  virtual void OnChannelError();
  virtual void OnMessageReceived(const IPC::Message& message);

  void OnRendererThreadInit(MessageLoop* message_loop);

  // Called by GpuChannelHost when a GPU channel is established.
  void OnGpuChannelConnected(GpuChannelHost* channel_host,
                             MessageRouter* router,
                             IPC::SyncChannel* channel);

  // Called on RenderThread to create a hardware accelerated video decoder
  // in the GPU process.
  //
  // A routing ID for the GLES2 context needs to be provided when creating a
  // hardware video decoder. This is important because the resources used by
  // the video decoder needs to be shared with the GLES2 context corresponding
  // to the RenderView.
  //
  // This means that a GPU video decoder is tied to a specific RenderView and
  // its GLES2 context in the GPU process.
  //
  // Returns a GpuVideoDecoderHost as a handle to control the video decoder.
  GpuVideoDecoderHost* CreateVideoDecoder(int context_route_id);

  // TODO(hclam): Provide a method to destroy the decoder. We also need to
  // listen to context lost event.

  // Methods called by GpuVideoDecoderHost.
  void AddRoute(int route_id, GpuVideoDecoderHost* decoder_host);
  void RemoveRoute(int route_id);

 private:
  GpuVideoServiceHost();

  GpuChannelHost* channel_host_;
  MessageRouter* router_;
  GpuVideoServiceInfoParam service_info_;
  MessageLoop* message_loop_;  // Message loop of render thread.

  friend struct DefaultSingletonTraits<GpuVideoServiceHost>;
  DISALLOW_COPY_AND_ASSIGN(GpuVideoServiceHost);
};

#endif  // CHROME_RENDERER_GPU_VIDEO_SERVICE_HOST_H_
