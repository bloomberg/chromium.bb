// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "ipc/ipc_listener.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class GpuChannelHost;

// This class is used to talk to VideoDecodeAccelerator in the Gpu process
// through IPC messages.
class GpuVideoDecodeAcceleratorHost
    : public IPC::Listener,
      public media::VideoDecodeAccelerator,
      public CommandBufferProxyImpl::DeletionObserver,
      public base::NonThreadSafe {
 public:
  // |this| is guaranteed not to outlive |channel| and |impl|.  (See comments
  // for |channel_| and |impl_|.)
  GpuVideoDecodeAcceleratorHost(GpuChannelHost* channel,
                                CommandBufferProxyImpl* impl);

  // IPC::Listener implementation.
  void OnChannelError() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // media::VideoDecodeAccelerator implementation.
  bool Initialize(const Config& config, Client* client) override;
  void SetCdm(int cdm_id) override;
  void Decode(const media::BitstreamBuffer& bitstream_buffer) override;
  void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;

  // CommandBufferProxyImpl::DeletionObserver implemetnation.
  void OnWillDeleteImpl() override;

 private:
  // Only Destroy() should be deleting |this|.
  ~GpuVideoDecodeAcceleratorHost() override;

  // Notify |client_| of an error.  Posts a task to avoid re-entrancy.
  void PostNotifyError(Error);

  void Send(IPC::Message* message);

  // IPC handlers, proxying media::VideoDecodeAccelerator::Client for the GPU
  // process.  Should not be called directly.
  void OnCdmAttached(bool success);
  void OnBitstreamBufferProcessed(int32_t bitstream_buffer_id);
  void OnProvidePictureBuffer(uint32_t num_requested_buffers,
                              const gfx::Size& dimensions,
                              uint32_t texture_target);
  void OnDismissPictureBuffer(int32_t picture_buffer_id);
  void OnPictureReady(int32_t picture_buffer_id,
                      int32_t bitstream_buffer_id,
                      const gfx::Rect& visible_rect,
                      bool allow_overlay);
  void OnFlushDone();
  void OnResetDone();
  void OnNotifyError(uint32_t error);

  // Unowned reference to the GpuChannelHost to send IPC messages to the GPU
  // process.  |channel_| outlives |impl_|, so the reference is always valid as
  // long as it is not NULL.
  GpuChannelHost* channel_;

  // Route ID for the associated decoder in the GPU process.
  int32_t decoder_route_id_;

  // The client that will receive callbacks from the decoder.
  Client* client_;

  // Unowned reference to the CommandBufferProxyImpl that created us.  |this|
  // registers as a DeletionObserver of |impl_|, the so reference is always
  // valid as long as it is not NULL.
  CommandBufferProxyImpl* impl_;

  // Requested dimensions of the buffer, from ProvidePictureBuffers().
  gfx::Size picture_buffer_dimensions_;

  // WeakPtr factory for posting tasks back to itself.
  base::WeakPtrFactory<GpuVideoDecodeAcceleratorHost> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecodeAcceleratorHost);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
