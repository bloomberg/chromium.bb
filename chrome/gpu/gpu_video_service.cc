// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_channel.h"
#include "chrome/gpu/gpu_video_decoder.h"
#include "chrome/gpu/gpu_video_service.h"

GpuVideoService::GpuVideoService() : next_available_decoder_id_(0) {
  // TODO(jiesun): move this time consuming stuff out of here.
  IntializeGpuVideoService();
}
GpuVideoService::~GpuVideoService() {
  // TODO(jiesun): move this time consuming stuff out of here.
  UnintializeGpuVideoService();
}

void GpuVideoService::OnChannelConnected(int32 peer_pid) {
  LOG(ERROR) << "GpuVideoService::OnChannelConnected";
}

void GpuVideoService::OnChannelError() {
  LOG(ERROR) << "GpuVideoService::OnChannelError";
}

void GpuVideoService::OnMessageReceived(const IPC::Message& msg) {
#if 0
  IPC_BEGIN_MESSAGE_MAP(GpuVideoService, msg)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
#endif
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
    GpuVideoDecoderInfoParam* param,
    gpu::gles2::GLES2Decoder* gles2_decoder) {
  GpuVideoDecoderInfo decoder_info;
  int32 decoder_id = GetNextAvailableDecoderID();
  param->decoder_id = decoder_id;
  base::ProcessHandle handle = channel->renderer_handle();
  decoder_info.decoder_ = new GpuVideoDecoder(param, channel, handle,
                                              gles2_decoder);
  decoder_info.channel_ = channel;
  decoder_info.param = *param;
  decoder_map_[decoder_id] = decoder_info;
  router->AddRoute(param->decoder_route_id, decoder_info.decoder_);
  return true;
}

void GpuVideoService::DestroyVideoDecoder(
    MessageRouter* router,
    int32 decoder_id) {
  int32 route_id = decoder_map_[decoder_id].param.decoder_route_id;
  router->RemoveRoute(route_id);
  decoder_map_.erase(decoder_id);
}

int32 GpuVideoService::GetNextAvailableDecoderID() {
  return ++next_available_decoder_id_;
}
