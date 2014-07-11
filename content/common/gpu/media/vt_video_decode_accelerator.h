// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_

#include "base/basictypes.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "content/common/gpu/media/vt.h"
#include "media/filters/h264_parser.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context_cgl.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace content {

// VideoToolbox.framework implementation of the VideoDecodeAccelerator
// interface for Mac OS X (currently limited to 10.9+).
class VTVideoDecodeAccelerator
    : public media::VideoDecodeAccelerator,
      public base::NonThreadSafe {
 public:
  explicit VTVideoDecodeAccelerator(CGLContextObj cgl_context);
  virtual ~VTVideoDecodeAccelerator();

  // VideoDecodeAccelerator implementation.
  virtual bool Initialize(
      media::VideoCodecProfile profile,
      Client* client) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& pictures) OVERRIDE;
  virtual void ReusePictureBuffer(int32_t picture_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool CanDecodeOnIOThread() OVERRIDE;

  // Called by VideoToolbox when a frame is decoded.
  void Output(
      int32_t bitstream_id,
      OSStatus status,
      VTDecodeInfoFlags info_flags,
      CVImageBufferRef image_buffer);

 private:
  // Configure a VideoToolbox decompression session from parameter set NALUs.
  void ConfigureDecoder(
      const std::vector<const uint8_t*>& nalu_data_ptrs,
      const std::vector<size_t>& nalu_data_sizes);

  // Decode a frame of bitstream.
  void DecodeTask(const media::BitstreamBuffer);

  CGLContextObj cgl_context_;
  media::VideoDecodeAccelerator::Client* client_;
  base::Thread decoder_thread_;

  // Decoder configuration (used only on decoder thread).
  VTDecompressionOutputCallbackRecord callback_;
  base::ScopedCFTypeRef<CMFormatDescriptionRef> format_;
  base::ScopedCFTypeRef<VTDecompressionSessionRef> session_;
  media::H264Parser parser_;
  gfx::Size coded_size_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to Controller are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<VTVideoDecodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VTVideoDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_
