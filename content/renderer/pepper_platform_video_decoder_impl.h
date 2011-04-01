// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLATFORM_VIDEO_DECODER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PLATFORM_VIDEO_DECODER_IMPL_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "media/base/data_buffer.h"
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
  virtual const std::vector<uint32>& GetConfig(
      const std::vector<uint32>& prototype_config);
  virtual bool Initialize(const std::vector<uint32>& config);
  virtual bool Decode(media::BitstreamBuffer* bitstream_buffer,
                      media::VideoDecodeAcceleratorCallback* callback);
  virtual void AssignPictureBuffer(
      std::vector<media::VideoDecodeAccelerator::PictureBuffer*>
          picture_buffers);
  virtual void ReusePictureBuffer(
      media::VideoDecodeAccelerator::PictureBuffer* picture_buffer);
  virtual bool Flush(media::VideoDecodeAcceleratorCallback* callback);
  virtual bool Abort(media::VideoDecodeAcceleratorCallback* callback);

  // VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(
      uint32 requested_num_of_buffers,
      const std::vector<uint32>& buffer_properties) OVERRIDE;
  virtual void PictureReady(
      media::VideoDecodeAccelerator::Picture* picture) OVERRIDE;
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
