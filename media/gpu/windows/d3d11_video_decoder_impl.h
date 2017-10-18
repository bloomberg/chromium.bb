// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_D3D11_VIDEO_DECODER_IMPL_H_
#define MEDIA_GPU_D3D11_VIDEO_DECODER_IMPL_H_

#include <d3d11.h>

#include <list>
#include <memory>
#include <string>
#include <tuple>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/win/scoped_comptr.h"
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#include "media/base/video_decoder.h"
#include "media/gpu/d3d11_h264_accelerator.h"
#include "media/gpu/gles2_decoder_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/output_with_release_mailbox_cb.h"

namespace media {

class MEDIA_GPU_EXPORT D3D11VideoDecoderImpl : public VideoDecoder,
                                               public D3D11VideoDecoderClient {
 public:
  D3D11VideoDecoderImpl(
      base::Callback<gpu::GpuCommandBufferStub*()> get_stub_cb,
      OutputWithReleaseMailboxCB output_cb);
  ~D3D11VideoDecoderImpl() override;

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

  // D3D11VideoDecoderClient implementation.
  D3D11PictureBuffer* GetPicture() override;
  void OutputResult(D3D11PictureBuffer* buffer,
                    size_t input_buffer_id) override;
  size_t input_buffer_id() const override;

  // Return a weak ptr, since D3D11VideoDecoder constructs callbacks for us.
  base::WeakPtr<D3D11VideoDecoderImpl> GetWeakPtr();

 private:
  void DoDecode();
  void CreatePictureBuffers();

  void OnMailboxReleased(D3D11PictureBuffer* buffer,
                         const gpu::SyncToken& sync_token);

  base::Callback<gpu::GpuCommandBufferStub*()> get_stub_cb_;
  gpu::GpuCommandBufferStub* stub_ = nullptr;
  // A helper for creating textures. Only valid while |stub_| is valid.
  std::unique_ptr<GLES2DecoderHelper> decoder_helper_;

  base::win::ScopedComPtr<ID3D11Device> device_;
  base::win::ScopedComPtr<ID3D11DeviceContext> device_context_;
  base::win::ScopedComPtr<ID3D11VideoDevice> video_device_;
  base::win::ScopedComPtr<ID3D11VideoContext> video_context_;

  std::unique_ptr<AcceleratedVideoDecoder> decoder_;
  std::unique_ptr<D3D11H264Accelerator> h264_accelerator_;

  GUID decoder_guid_;

  std::list<std::pair<scoped_refptr<DecoderBuffer>, DecodeCB>>
      input_buffer_queue_;
  scoped_refptr<DecoderBuffer> current_buffer_;
  DecodeCB current_decode_cb_;
  base::TimeDelta current_timestamp_;

  std::vector<std::unique_ptr<D3D11PictureBuffer>> picture_buffers_;

  OutputWithReleaseMailboxCB output_cb_;

  base::WeakPtrFactory<D3D11VideoDecoderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(D3D11VideoDecoderImpl);
};

}  // namespace media

#endif  // MEDIA_GPU_D3D11_VIDEO_DECODER_IMPL_H_
