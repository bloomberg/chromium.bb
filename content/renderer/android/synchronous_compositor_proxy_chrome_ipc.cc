// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_proxy_chrome_ipc.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/shared_memory.h"
#include "cc/ipc/cc_param_traits.h"
#include "content/common/android/sync_compositor_statics.h"
#include "content/common/input/sync_compositor_messages.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/skia_util.h"

namespace content {

SynchronousCompositorProxyChromeIPC::SynchronousCompositorProxyChromeIPC(
    int routing_id,
    IPC::Sender* sender,
    ui::SynchronousInputHandlerProxy* input_handler_proxy)
    : SynchronousCompositorProxy(input_handler_proxy),
      routing_id_(routing_id),
      sender_(sender) {}

SynchronousCompositorProxyChromeIPC::~SynchronousCompositorProxyChromeIPC() {}

void SynchronousCompositorProxyChromeIPC::Send(IPC::Message* message) {
  sender_->Send(message);
}

void SynchronousCompositorProxyChromeIPC::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorProxyChromeIPC, message)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_ComputeScroll, ComputeScroll)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncCompositorMsg_DemandDrawHw,
                                    OnDemandDrawHw)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_DemandDrawHwAsync, DemandDrawHwAsync)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_SetSharedMemory, OnSetSharedMemory)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_ZeroSharedMemory, ZeroSharedMemory)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncCompositorMsg_DemandDrawSw,
                                    OnDemandDrawSw)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_ZoomBy, OnZoomBy)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_SetScroll, SetScroll)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_SetMemoryPolicy, SetMemoryPolicy)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_ReclaimResources, ReclaimResources)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_SetBeginFramePaused,
                        SetBeginFrameSourcePaused)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_BeginFrame, BeginFrame)
  IPC_END_MESSAGE_MAP()
}

void SynchronousCompositorProxyChromeIPC::OnDemandDrawHw(
    const SyncCompositorDemandDrawHwParams& params,
    IPC::Message* reply_message) {
  DCHECK(reply_message);
  DemandDrawHw(params,
               base::BindOnce(
                   &SynchronousCompositorProxyChromeIPC::SendDemandDrawHwReply,
                   base::Unretained(this), reply_message));
}

void SynchronousCompositorProxyChromeIPC::OnDemandDrawSw(
    const SyncCompositorDemandDrawSwParams& params,
    IPC::Message* reply_message) {
  DemandDrawSw(params,
               base::BindOnce(
                   &SynchronousCompositorProxyChromeIPC::SendDemandDrawSwReply,
                   base::Unretained(this), reply_message));
}

void SynchronousCompositorProxyChromeIPC::SendDemandDrawHwAsyncReply(
    const SyncCompositorCommonRendererParams& common_renderer_params,
    uint32_t layer_tree_frame_sink_id,
    uint32_t metadata_version,
    base::Optional<viz::CompositorFrame> frame) {
  Send(new SyncCompositorHostMsg_ReturnFrame(
      routing_id_, layer_tree_frame_sink_id, metadata_version, frame));
}

void SynchronousCompositorProxyChromeIPC::SendDemandDrawHwReply(
    IPC::Message* reply_message,
    const SyncCompositorCommonRendererParams& common_renderer_params,
    uint32_t layer_tree_frame_sink_id,
    uint32_t metadata_version,
    base::Optional<viz::CompositorFrame> frame) {
  SyncCompositorMsg_DemandDrawHw::WriteReplyParams(
      reply_message, common_renderer_params, layer_tree_frame_sink_id,
      metadata_version, frame);
  Send(reply_message);
}

void SynchronousCompositorProxyChromeIPC::SendDemandDrawSwReply(
    IPC::Message* reply_message,
    const SyncCompositorCommonRendererParams& common_renderer_params,
    uint32_t metadata_version,
    base::Optional<viz::CompositorFrameMetadata> metadata) {
  SyncCompositorMsg_DemandDrawSw::WriteReplyParams(
      reply_message, common_renderer_params, metadata_version, metadata);
  Send(reply_message);
}

void SynchronousCompositorProxyChromeIPC::SendAsyncRendererStateIfNeeded() {
  // If any of the sync IPCs are in flight avoid pushing an asynch update
  // state message as that is pointless as we will send the updated state
  // with the response when the compositor frame is submitted.
  if (hardware_draw_reply_ || software_draw_reply_ || zoom_by_reply_)
    return;
  SyncCompositorCommonRendererParams params;
  PopulateCommonParams(&params);

  Send(new SyncCompositorHostMsg_UpdateState(routing_id_, params));
}

void SynchronousCompositorProxyChromeIPC::OnSetSharedMemory(
    const SyncCompositorSetSharedMemoryParams& params,
    bool* success,
    SyncCompositorCommonRendererParams* common_renderer_params) {
  *success = false;
  SetSharedMemory(
      params,
      base::BindOnce(&SynchronousCompositorProxyChromeIPC::SetSharedMemoryReply,
                     base::Unretained(this), success, common_renderer_params));
}

void SynchronousCompositorProxyChromeIPC::SetSharedMemoryReply(
    bool* out_success,
    SyncCompositorCommonRendererParams* out_common_renderer_params,
    bool success,
    const SyncCompositorCommonRendererParams& common_renderer_params) {
  *out_success = success;
  *out_common_renderer_params = common_renderer_params;
}

void SynchronousCompositorProxyChromeIPC::OnZoomBy(
    float zoom_delta,
    const gfx::Point& anchor,
    SyncCompositorCommonRendererParams* common_renderer_params) {
  ZoomBy(zoom_delta, anchor,
         base::BindOnce(&SynchronousCompositorProxyChromeIPC::ZoomByReply,
                        base::Unretained(this), common_renderer_params));
}

void SynchronousCompositorProxyChromeIPC::ZoomByReply(
    SyncCompositorCommonRendererParams* output_params,
    const SyncCompositorCommonRendererParams& params) {
  *output_params = params;
}

void SynchronousCompositorProxyChromeIPC::LayerTreeFrameSinkCreated() {
  Send(new SyncCompositorHostMsg_LayerTreeFrameSinkCreated(routing_id_));
}

void SynchronousCompositorProxyChromeIPC::SendBeginFrameResponse(
    const content::SyncCompositorCommonRendererParams& param) {
  Send(new SyncCompositorHostMsg_BeginFrameResponse(routing_id_, param));
}

void SynchronousCompositorProxyChromeIPC::SendSetNeedsBeginFrames(
    bool needs_begin_frames) {
  Send(new SyncCompositorHostMsg_SetNeedsBeginFrames(routing_id_,
                                                     needs_begin_frames));
}

}  // namespace content
