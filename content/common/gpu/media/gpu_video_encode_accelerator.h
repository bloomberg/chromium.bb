// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_ENCODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "gpu/config/gpu_info.h"
#include "ipc/ipc_listener.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

struct AcceleratedVideoEncoderMsg_Encode_Params;
struct AcceleratedVideoEncoderMsg_Encode_Params2;

namespace base {
class SharedMemory;
}  // namespace base

namespace content {

// This class encapsulates the GPU process view of a VideoEncodeAccelerator,
// wrapping the platform-specific VideoEncodeAccelerator instance.  It handles
// IPC coming in from the renderer and passes it to the underlying VEA.
class GpuVideoEncodeAccelerator
    : public IPC::Listener,
      public media::VideoEncodeAccelerator::Client,
      public GpuCommandBufferStub::DestructionObserver {
 public:
  GpuVideoEncodeAccelerator(int32_t host_route_id, GpuCommandBufferStub* stub);
  ~GpuVideoEncodeAccelerator() override;

  // Initialize this accelerator with the given parameters and send
  // |init_done_msg| when complete.
  void Initialize(media::VideoPixelFormat input_format,
                  const gfx::Size& input_visible_size,
                  media::VideoCodecProfile output_profile,
                  uint32_t initial_bitrate,
                  IPC::Message* init_done_msg);

  // IPC::Listener implementation
  bool OnMessageReceived(const IPC::Message& message) override;

  // media::VideoEncodeAccelerator::Client implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            size_t payload_size,
                            bool key_frame) override;
  void NotifyError(media::VideoEncodeAccelerator::Error error) override;

  // GpuCommandBufferStub::DestructionObserver implementation.
  void OnWillDestroyStub() override;

  // Static query for supported profiles.  This query calls the appropriate
  // platform-specific version. The returned supported profiles vector will
  // not contain duplicates.
  static gpu::VideoEncodeAcceleratorSupportedProfiles GetSupportedProfiles();

 private:
  typedef scoped_ptr<media::VideoEncodeAccelerator>(*CreateVEAFp)();

  // Return a set of VEA Create function pointers applicable to the current
  // platform.
  static std::vector<CreateVEAFp> CreateVEAFps();
  static scoped_ptr<media::VideoEncodeAccelerator> CreateV4L2VEA();
  static scoped_ptr<media::VideoEncodeAccelerator> CreateVaapiVEA();
  static scoped_ptr<media::VideoEncodeAccelerator> CreateAndroidVEA();

  // IPC handlers, proxying media::VideoEncodeAccelerator for the renderer
  // process.
  void OnEncode(const AcceleratedVideoEncoderMsg_Encode_Params& params);
  void OnEncode2(const AcceleratedVideoEncoderMsg_Encode_Params2& params);
  void OnUseOutputBitstreamBuffer(int32_t buffer_id,
                                  base::SharedMemoryHandle buffer_handle,
                                  uint32_t buffer_size);
  void OnRequestEncodingParametersChange(uint32_t bitrate, uint32_t framerate);

  void OnDestroy();

  void EncodeFrameFinished(int32_t frame_id,
                           scoped_ptr<base::SharedMemory> shm);
  void EncodeFrameFinished2(int32_t frame_id,
                            ScopedVector<gfx::GpuMemoryBuffer> buffers);
  void Send(IPC::Message* message);
  // Helper for replying to the creation request.
  void SendCreateEncoderReply(IPC::Message* message, bool succeeded);

  // Route ID to communicate with the host.
  const uint32_t host_route_id_;

  // Unowned pointer to the underlying GpuCommandBufferStub.  |this| is
  // registered as a DestuctionObserver of |stub_| and will self-delete when
  // |stub_| is destroyed.
  GpuCommandBufferStub* const stub_;

  // Owned pointer to the underlying VideoEncodeAccelerator.
  scoped_ptr<media::VideoEncodeAccelerator> encoder_;
  base::Callback<bool(void)> make_context_current_;

  // Video encoding parameters.
  media::VideoPixelFormat input_format_;
  gfx::Size input_visible_size_;
  gfx::Size input_coded_size_;
  size_t output_buffer_size_;

  // Weak pointer for media::VideoFrames that refer back to |this|.
  base::WeakPtrFactory<GpuVideoEncodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoEncodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_ENCODE_ACCELERATOR_H_
