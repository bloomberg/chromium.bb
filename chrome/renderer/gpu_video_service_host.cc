// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/gpu_video_service_host.h"

#include "chrome/common/gpu_messages.h"
#include "chrome/renderer/gpu_video_decoder_host.h"
#include "chrome/renderer/render_thread.h"

GpuVideoServiceHost::GpuVideoServiceHost()
    : channel_host_(NULL),
      router_(NULL),
      message_loop_(NULL) {
  memset(&service_info_, 0, sizeof(service_info_));
}

void GpuVideoServiceHost::OnChannelError() {
  LOG(ERROR) << "GpuVideoServiceHost::OnChannelError";
  channel_host_ = NULL;
  router_ = NULL;
}

void GpuVideoServiceHost::OnMessageReceived(const IPC::Message& msg) {
#if 0
  IPC_BEGIN_MESSAGE_MAP(GpuVideoServiceHost, msg)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
#endif
}

void GpuVideoServiceHost::OnRendererThreadInit(MessageLoop* message_loop) {
  message_loop_ = message_loop;
}

void GpuVideoServiceHost::OnGpuChannelConnected(
    GpuChannelHost* channel_host,
    MessageRouter* router,
    IPC::SyncChannel* channel) {

  channel_host_ = channel_host;
  router_ = router;

  // Get the routing_id of video service in GPU process.
  service_info_.service_available = 0;
  if (!channel_host_->Send(new GpuChannelMsg_GetVideoService(&service_info_))) {
    LOG(ERROR) << "GpuChannelMsg_GetVideoService failed";
  }

  if (service_info_.service_available)
    router->AddRoute(service_info_.video_service_host_route_id, this);
}

GpuVideoDecoderHost* GpuVideoServiceHost::CreateVideoDecoder(
    int context_route_id) {
  DCHECK(RenderThread::current());

  return new GpuVideoDecoderHost(this, channel_host_, context_route_id);
}

void GpuVideoServiceHost::AddRoute(int route_id,
                                   GpuVideoDecoderHost* decoder_host) {
  router_->AddRoute(route_id, decoder_host);
}

void GpuVideoServiceHost::RemoveRoute(int route_id) {
  router_->RemoveRoute(route_id);
}
