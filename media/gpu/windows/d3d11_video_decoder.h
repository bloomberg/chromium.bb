// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_D3D11_VIDEO_DECODER_H_
#define MEDIA_GPU_D3D11_VIDEO_DECODER_H_

#include <d3d11.h>

#include <string>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#include "media/base/video_decoder.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_video_decoder_impl.h"
#include "media/gpu/windows/output_with_release_mailbox_cb.h"

namespace media {

// Thread-hopping implementation of D3D11VideoDecoder.  It's meant to run on
// a random thread, and hop to the gpu main thread.  It does this so that it
// can use the D3D context etc.  What should really happen is that we should
// get (or share with other D3D11VideoDecoder instances) our own context, and
// just share the D3D texture with the main thread's context.  However, for
// now, it's easier to hop threads.
class MEDIA_GPU_EXPORT D3D11VideoDecoder : public VideoDecoder {
 public:
  D3D11VideoDecoder(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
                    base::Callback<gpu::GpuCommandBufferStub*()> get_stub_cb,
                    OutputWithReleaseMailboxCB output_cb);
  ~D3D11VideoDecoder() override;

  // VideoDecoder implementation:
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

 private:
  // Call |output_cb| with a release cb that will hop back to the impl thread
  // to run |impl_thread_cb| when |video_frame| is released.
  void OutputWithThreadHoppingRelease(
      OutputWithReleaseMailboxCB output_cb,
      VideoFrame::ReleaseMailboxCB impl_thread_cb,
      const scoped_refptr<VideoFrame>& video_frame);

  // ReleaseCB that's run on our thread, but posts it to the impl thread.
  void OnMailboxReleased(VideoFrame::ReleaseMailboxCB impl_thread_cb,
                         const gpu::SyncToken& token);

  // The implementation, which we trampoline to the impl thread.
  // This must be freed on the impl thread.
  std::unique_ptr<D3D11VideoDecoderImpl> impl_;

  // Weak ptr to |impl_|, which we use for callbacks.
  base::WeakPtr<VideoDecoder> impl_weak_;

  // Task runner for |impl_|.  This must be the GPU main thread.
  scoped_refptr<base::SequencedTaskRunner> impl_task_runner_;

  OutputWithReleaseMailboxCB output_cb_;

  base::WeakPtrFactory<D3D11VideoDecoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(D3D11VideoDecoder);
};

}  // namespace media

#endif  // MEDIA_GPU_D3D11_VIDEO_DECODER_H_
