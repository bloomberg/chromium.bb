// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_channel.h"
#include "chrome/gpu/gpu_video_decoder.h"
#include "chrome/gpu/gpu_video_service.h"
#include "content/common/gpu_messages.h"

struct GpuVideoService::GpuVideoDecoderInfo {
  scoped_refptr<GpuVideoDecoder> decoder;
  GpuChannel* channel;
};


GpuVideoService::GpuVideoService() {
  // TODO(jiesun): move this time consuming stuff out of here.
  IntializeGpuVideoService();
}

GpuVideoService::~GpuVideoService() {
  // TODO(jiesun): move this time consuming stuff out of here.
  UnintializeGpuVideoService();
}

// static
GpuVideoService* GpuVideoService::GetInstance() {
  return Singleton<GpuVideoService>::get();
}

void GpuVideoService::OnChannelConnected(int32 peer_pid) {
  LOG(ERROR) << "GpuVideoService::OnChannelConnected";
}

void GpuVideoService::OnChannelError() {
  LOG(ERROR) << "GpuVideoService::OnChannelError";
}

bool GpuVideoService::OnMessageReceived(const IPC::Message& msg) {
#if 0
  IPC_BEGIN_MESSAGE_MAP(GpuVideoService, msg)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
#endif
  return false;
}

bool GpuVideoService::IntializeGpuVideoService() {
  return true;
}

bool GpuVideoService::UnintializeGpuVideoService() {
  return true;
}

bool GpuVideoService::CreateVideoDecoder(
    GpuChannel* channel,
    MessageRouter* router,
    int32 decoder_host_id,
    int32 decoder_id,
    gpu::gles2::GLES2Decoder* gles2_decoder) {
  GpuVideoDecoderInfo decoder_info;
  decoder_info.decoder = new GpuVideoDecoder(MessageLoop::current(),
                                             decoder_host_id,
                                             channel,
                                             channel->renderer_process(),
                                             gles2_decoder);
  decoder_info.channel = channel;
  decoder_map_[decoder_id] = decoder_info;
  router->AddRoute(decoder_id, decoder_info.decoder);

  channel->Send(new GpuVideoDecoderHostMsg_CreateVideoDecoderDone(
      decoder_host_id, decoder_id));
  return true;
}

void GpuVideoService::DestroyVideoDecoder(
    MessageRouter* router,
    int32 decoder_id) {
  router->RemoveRoute(decoder_id);
  decoder_map_.erase(decoder_id);
}
