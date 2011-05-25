// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_video_service.h"

#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_video_decode_accelerator.h"

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
    const std::vector<uint32>& configs) {
  // Create GpuVideoDecodeAccelerator and add to map.
  scoped_refptr<GpuVideoDecodeAccelerator> decoder =
      new GpuVideoDecodeAccelerator(channel, decoder_host_id);

  bool result = decoder_map_.insert(std::make_pair(decoder_id, decoder)).second;

  // Decoder ID is a unique ID determined by GpuVideoServiceHost.
  // We should always be adding entries here.
  DCHECK(result);

  router->AddRoute(decoder_id, decoder);

  // Tell client that initialization is complete.
  channel->Send(
      new AcceleratedVideoDecoderHostMsg_CreateDone(
          decoder_host_id, decoder_id));

  return true;
}

void GpuVideoService::DestroyVideoDecoder(
    MessageRouter* router,
    int32 decoder_id) {
  router->RemoveRoute(decoder_id);
  decoder_map_.erase(decoder_id);
}
