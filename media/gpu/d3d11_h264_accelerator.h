// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_D3D11_H264_ACCELERATOR_H_
#define MEDIA_GPU_D3D11_H264_ACCELERATOR_H_

#include <d3d11.h>
#include <d3d9.h>
#include <dxva.h>

#include <vector>

#include "base/win/scoped_comptr.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/h264_dpb.h"
#include "media/video/picture.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"

namespace media {
class D3D11H264Accelerator;

class D3D11PictureBuffer {
 public:
  D3D11PictureBuffer(PictureBuffer picture_buffer, size_t level);
  ~D3D11PictureBuffer();

  bool Init(base::win::ScopedComPtr<ID3D11VideoDevice> video_device,
            base::win::ScopedComPtr<ID3D11Texture2D> texture,
            const GUID& decoder_guid);

  size_t level() const { return level_; }

  PictureBuffer& picture_buffer() { return picture_buffer_; }

  bool in_client_use() const { return in_client_use_; }
  bool in_picture_use() const { return in_picture_use_; }
  void set_in_client_use(bool use) { in_client_use_ = use; }
  void set_in_picture_use(bool use) { in_picture_use_ = use; }

 private:
  friend class D3D11H264Accelerator;

  PictureBuffer picture_buffer_;
  base::win::ScopedComPtr<ID3D11Texture2D> texture_;
  bool in_picture_use_ = false;
  bool in_client_use_ = false;
  size_t level_;
  base::win::ScopedComPtr<ID3D11VideoDecoderOutputView> output_view_;
  EGLStreamKHR stream_;

  DISALLOW_COPY_AND_ASSIGN(D3D11PictureBuffer);
};

class D3D11VideoDecoderClient {
 public:
  virtual size_t input_buffer_id() const = 0;
  virtual D3D11PictureBuffer* GetPicture() = 0;
  virtual void OutputResult(D3D11PictureBuffer* picture,
                            size_t input_buffer_id) = 0;
};

class D3D11H264Accelerator : public H264Decoder::H264Accelerator {
 public:
  D3D11H264Accelerator(
      D3D11VideoDecoderClient* client,
      base::win::ScopedComPtr<ID3D11VideoDecoder> video_decoder,
      base::win::ScopedComPtr<ID3D11VideoDevice> video_device,
      base::win::ScopedComPtr<ID3D11VideoContext> video_context);
  ~D3D11H264Accelerator() override;

  // H264Decoder::H264Accelerator implementation.
  scoped_refptr<H264Picture> CreateH264Picture() override;
  bool SubmitFrameMetadata(const H264SPS* sps,
                           const H264PPS* pps,
                           const H264DPB& dpb,
                           const H264Picture::Vector& ref_pic_listp0,
                           const H264Picture::Vector& ref_pic_listb0,
                           const H264Picture::Vector& ref_pic_listb1,
                           const scoped_refptr<H264Picture>& pic) override;
  bool SubmitSlice(const H264PPS* pps,
                   const H264SliceHeader* slice_hdr,
                   const H264Picture::Vector& ref_pic_list0,
                   const H264Picture::Vector& ref_pic_list1,
                   const scoped_refptr<H264Picture>& pic,
                   const uint8_t* data,
                   size_t size) override;
  bool SubmitDecode(const scoped_refptr<H264Picture>& pic) override;
  void Reset() override {}
  bool OutputPicture(const scoped_refptr<H264Picture>& pic) override;

 private:
  void SubmitSliceData();
  void RetrieveBitstreamBuffer();

  D3D11VideoDecoderClient* client_;

  base::win::ScopedComPtr<ID3D11VideoDecoder> video_decoder_;
  base::win::ScopedComPtr<ID3D11VideoDevice> video_device_;
  base::win::ScopedComPtr<ID3D11VideoContext> video_context_;

  // This information set at the beginning of a frame and saved for processing
  // all the slices.
  DXVA_PicEntry_H264 ref_frame_list_[16];
  H264SPS sps_;
  INT field_order_cnt_list_[16][2];
  USHORT frame_num_list_[16];
  UINT used_for_reference_flags_;
  USHORT non_existing_frame_flags_;

  // Information that's accumulated during slices and submitted at the end
  std::vector<DXVA_Slice_H264_Short> slice_info_;
  size_t current_offset_ = 0;
  size_t bitstream_buffer_size_;
  uint8_t* bitstream_buffer_bytes_;

  DISALLOW_COPY_AND_ASSIGN(D3D11H264Accelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_D3D11_H264_ACCELERATOR_H_
