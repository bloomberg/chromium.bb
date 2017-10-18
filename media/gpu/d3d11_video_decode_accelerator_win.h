// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_D3D11_VIDEO_DECODE_ACCELERATOR_WIN_H_
#define MEDIA_GPU_D3D11_VIDEO_DECODE_ACCELERATOR_WIN_H_

#include <d3d11.h>
#include <wrl/client.h>

#include <list>
#include <memory>

#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/d3d11_h264_accelerator.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"

namespace media {

class MEDIA_GPU_EXPORT D3D11VideoDecodeAccelerator
    : public VideoDecodeAccelerator,
      public D3D11VideoDecoderClient {
 public:
  D3D11VideoDecodeAccelerator(
      const GetGLContextCallback& get_gl_context_cb,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb);

  ~D3D11VideoDecodeAccelerator() override;

  // VideoDecodeAccelerator implementation.
  bool Initialize(const Config& config, Client* client) override;
  void Decode(const BitstreamBuffer& bitstream_buffer) override;
  void DoDecode();
  void AssignPictureBuffers(const std::vector<PictureBuffer>& buffers) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool TryToSetupDecodeOnSeparateThread(
      const base::WeakPtr<Client>& decode_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner)
      override;
  GLenum GetSurfaceInternalFormat() const override;

  // D3D11VideoDecoderClient implementation.
  D3D11PictureBuffer* GetPicture() override;
  void OutputResult(D3D11PictureBuffer* buffer,
                    size_t input_buffer_id) override;
  size_t input_buffer_id() const override;

 private:
  Client* client_;
  GetGLContextCallback get_gl_context_cb_;
  MakeGLContextCurrentCallback make_context_current_cb_;
  BindGLImageCallback bind_image_cb_;
  Microsoft::WRL::ComPtr<ID3D11Device> device_;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context_;
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
  std::unique_ptr<AcceleratedVideoDecoder> decoder_;
  std::unique_ptr<D3D11H264Accelerator> h264_accelerator_;

  GUID decoder_guid_;

  std::list<BitstreamBuffer> input_buffer_queue_;
  uint32_t input_buffer_id_;
  std::unique_ptr<base::SharedMemory> bitstream_buffer_;
  size_t bitstream_buffer_size_;

  std::vector<std::unique_ptr<D3D11PictureBuffer>> picture_buffers_;

  DISALLOW_COPY_AND_ASSIGN(D3D11VideoDecodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_D3D11_VIDEO_DECODE_ACCELERATOR_WIN_H_
