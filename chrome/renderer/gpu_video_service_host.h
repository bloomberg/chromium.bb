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

class GpuVideoServiceHost : public IPC::Channel::Listener,
                            public Singleton<GpuVideoServiceHost> {
 public:
  // IPC::Channel::Listener.
  virtual void OnChannelConnected(int32 peer_pid) {}
  virtual void OnChannelError();
  virtual void OnMessageReceived(const IPC::Message& message);

  void OnRendererThreadInit(MessageLoop* message_loop);
  void OnGpuChannelConnected(GpuChannelHost* channel_host,
                             MessageRouter* router,
                             IPC::SyncChannel* channel);

  // call at RenderThread. one per renderer process.
  scoped_refptr<GpuVideoDecoderHost> CreateVideoDecoder(
      GpuVideoDecoderHost::EventHandler* event_handler);
  void DestroyVideoDecoder(scoped_refptr<GpuVideoDecoderHost>);

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

