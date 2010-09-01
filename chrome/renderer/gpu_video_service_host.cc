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

scoped_refptr<GpuVideoDecoderHost> GpuVideoServiceHost::CreateVideoDecoder(
    GpuVideoDecoderHost::EventHandler* event_handler) {
  DCHECK(RenderThread::current());

  if (!channel_host_ || !service_info_.service_available_)
    return NULL;

  GpuVideoDecoderInfoParam param;
  if (!channel_host_->Send(new GpuChannelMsg_CreateVideoDecoder(&param))) {
    LOG(ERROR) << "GpuChannelMsg_CreateVideoDecoder failed";
    return NULL;
  }

  scoped_refptr<GpuVideoDecoderHost> gpu_video_decoder_host =
      new GpuVideoDecoderHost(this, channel_host_, event_handler, param);
  if (!gpu_video_decoder_host.get()) {
    if (!channel_host_->Send(
        new GpuChannelMsg_DestroyVideoDecoder(param.decoder_id_))) {
      LOG(ERROR) << "GpuChannelMsg_DestroyVideoDecoder failed";
    }
    return NULL;
  }

  router_->AddRoute(gpu_video_decoder_host->my_route_id(),
                    gpu_video_decoder_host.get());
  return gpu_video_decoder_host;
}

void GpuVideoServiceHost::DestroyVideoDecoder(
    scoped_refptr<GpuVideoDecoderHost> gpu_video_decoder_host) {
  DCHECK(RenderThread::current());

  if (!channel_host_ || !service_info_.service_available_)
    return;

  DCHECK(gpu_video_decoder_host.get());

  int32 decoder_id = gpu_video_decoder_host->decoder_id();
  if (!channel_host_->Send(new GpuChannelMsg_DestroyVideoDecoder(decoder_id))) {
    LOG(ERROR) << "GpuChannelMsg_DestroyVideoDecoder failed";
  }

  router_->RemoveRoute(gpu_video_decoder_host->my_route_id());
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
  service_info_.service_available_ = 0;
  if (!channel_host_->Send(new GpuChannelMsg_GetVideoService(&service_info_))) {
    LOG(ERROR) << "GpuChannelMsg_GetVideoService failed";
  }

  if (service_info_.service_available_)
    router->AddRoute(service_info_.video_service_host_route_id_, this);
}

