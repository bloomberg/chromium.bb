// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_PROXY_CHROME_IPC_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_PROXY_CHROME_IPC_H_

#include "content/renderer/android/synchronous_compositor_proxy.h"

namespace IPC {
class Message;
class Sender;
}  // namespace IPC

namespace content {

// This class implements the SynchronousCompositorProxy with
// IPC messaging backed by Chrome IPC.
class SynchronousCompositorProxyChromeIPC : public SynchronousCompositorProxy {
 public:
  SynchronousCompositorProxyChromeIPC(
      int routing_id,
      IPC::Sender* sender,
      ui::SynchronousInputHandlerProxy* input_handler_proxy);
  ~SynchronousCompositorProxyChromeIPC() override;

  void Send(IPC::Message* message);
  void OnMessageReceived(const IPC::Message& message);

 protected:
  void SendSetNeedsBeginFrames(bool needs_begin_frames) final;
  void SendAsyncRendererStateIfNeeded() final;
  void LayerTreeFrameSinkCreated() final;
  void SendBeginFrameResponse(
      const content::SyncCompositorCommonRendererParams&) final;
  void SendDemandDrawHwAsyncReply(
      const content::SyncCompositorCommonRendererParams&,
      uint32_t layer_tree_frame_sink_id,
      uint32_t metadata_version,
      base::Optional<viz::CompositorFrame>) final;

 private:
  // IPC handlers.
  void OnSetSharedMemory(
      const SyncCompositorSetSharedMemoryParams& params,
      bool* success,
      SyncCompositorCommonRendererParams* common_renderer_params);
  void OnDemandDrawHw(const SyncCompositorDemandDrawHwParams& params,
                      IPC::Message* reply_message);
  void OnDemandDrawSw(const SyncCompositorDemandDrawSwParams& params,
                      IPC::Message* reply_message);

  void OnZoomBy(float zoom_delta,
                const gfx::Point& anchor,
                SyncCompositorCommonRendererParams* common_renderer_params);

  void SendDemandDrawHwReply(
      IPC::Message* reply_message,
      const SyncCompositorCommonRendererParams& common_renderer_params,
      uint32_t layer_tree_frame_sink_id,
      uint32_t metadata_version,
      base::Optional<viz::CompositorFrame> frame);
  void SendDemandDrawSwReply(
      IPC::Message* reply_message,
      const SyncCompositorCommonRendererParams& common_renderer_params,
      uint32_t metadata_version,
      base::Optional<viz::CompositorFrameMetadata> metadata);
  void ZoomByReply(SyncCompositorCommonRendererParams* output_params,
                   const SyncCompositorCommonRendererParams& params);
  void SetSharedMemoryReply(
      bool* out_success,
      SyncCompositorCommonRendererParams* out_common_renderer_params,
      bool success,
      const SyncCompositorCommonRendererParams& common_renderer_params);

  const int routing_id_;
  IPC::Sender* const sender_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorProxyChromeIPC);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_PROXY_CHROME_IPC_H_
