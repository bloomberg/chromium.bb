// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_VIDEO_LAYER_PROXY_H_
#define CHROME_BROWSER_RENDERER_HOST_VIDEO_LAYER_PROXY_H_

#include "chrome/browser/renderer_host/video_layer.h"
#include "ipc/ipc_channel.h"

class GpuProcessHostUIShim;

// Proxies YUV video layer data to the GPU process for rendering.
class VideoLayerProxy : public VideoLayer, public IPC::Channel::Listener {
 public:
  VideoLayerProxy(RenderWidgetHost* widget, const gfx::Size& size,
                  GpuProcessHostUIShim* process_shim, int32 routing_id);
  virtual ~VideoLayerProxy();

  // VideoLayer implementation.
  virtual void CopyTransportDIB(RenderProcessHost* process,
                                TransportDIB::Id bitmap,
                                const gfx::Rect& bitmap_rect);

  // IPC::Channel::Listener implementation.
  virtual void OnMessageReceived(const IPC::Message& message);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

 private:
  // Called when GPU process has finished painting the video layer.
  void OnPaintToVideoLayerACK();

  // GPU process receiving our proxied requests.
  GpuProcessHostUIShim* process_shim_;

  // IPC routing ID to use when communicating with the GPU process.
  int32 routing_id_;

  DISALLOW_COPY_AND_ASSIGN(VideoLayerProxy);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_VIDEO_LAYER_PROXY_H_
