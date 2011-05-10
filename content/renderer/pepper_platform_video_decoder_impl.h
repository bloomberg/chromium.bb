// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLATFORM_VIDEO_DECODER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PLATFORM_VIDEO_DECODER_IMPL_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "media/video/video_decode_accelerator.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

class PlatformVideoDecoderImpl
    : public webkit::ppapi::PluginDelegate::PlatformVideoDecoder,
      public media::VideoDecodeAccelerator::Client {
 public:
  explicit PlatformVideoDecoderImpl(
      media::VideoDecodeAccelerator* video_decode_accelerator);
  virtual ~PlatformVideoDecoderImpl();

  // PlatformVideoDecoder implementation.
  virtual void  GetConfigs(
      const std::vector<uint32>& requested_configs,
      std::vector<uint32>* matched_configs) OVERRIDE;
  virtual bool Initialize(const std::vector<uint32>& config) OVERRIDE;
  virtual bool Decode(
      const media::BitstreamBuffer& bitstream_buffer,
      media::VideoDecodeAcceleratorCallback* callback) OVERRIDE;
  virtual void AssignGLESBuffers(
      const std::vector<media::GLESBuffer>& buffers) OVERRIDE;
  virtual void AssignSysmemBuffers(
      const std::vector<media::SysmemBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(uint32 picture_buffer_id) OVERRIDE;
  virtual bool Flush(media::VideoDecodeAcceleratorCallback* callback) OVERRIDE;
  virtual bool Abort(media::VideoDecodeAcceleratorCallback* callback) OVERRIDE;

  // VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(
      uint32 requested_num_of_buffers,
      gfx::Size dimensions,
      media::VideoDecodeAccelerator::MemoryType type) OVERRIDE;
  virtual void PictureReady(
      const media::Picture& picture) OVERRIDE;
  virtual void NotifyEndOfStream() OVERRIDE;
  virtual void NotifyError(
      media::VideoDecodeAccelerator::Error error) OVERRIDE;

 private:
  // EventHandler lifetime must exceed lifetime of this class.
  media::VideoDecodeAccelerator::Client* client_;
  scoped_ptr<media::VideoDecodeAccelerator> video_decode_accelerator_;

  std::vector<uint32> configs;

  DISALLOW_COPY_AND_ASSIGN(PlatformVideoDecoderImpl);
};

#endif  // CONTENT_RENDERER_PEPPER_PLATFORM_VIDEO_DECODER_IMPL_H_
