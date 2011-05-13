// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLATFORM_VIDEO_DECODER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PLATFORM_VIDEO_DECODER_IMPL_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "media/video/video_decode_accelerator.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

class GpuChannelHost;

class PlatformVideoDecoderImpl
    : public webkit::ppapi::PluginDelegate::PlatformVideoDecoder,
      public media::VideoDecodeAccelerator::Client,
      public base::RefCountedThreadSafe<PlatformVideoDecoderImpl> {
 public:
  explicit PlatformVideoDecoderImpl(
      media::VideoDecodeAccelerator::Client* client);
  virtual ~PlatformVideoDecoderImpl();

  // PlatformVideoDecoder implementation.
  virtual void  GetConfigs(
      const std::vector<uint32>& requested_configs,
      std::vector<uint32>* matched_configs) OVERRIDE;
  virtual bool Initialize(const std::vector<uint32>& config) OVERRIDE;
  virtual bool Decode(
      const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignGLESBuffers(
      const std::vector<media::GLESBuffer>& buffers) OVERRIDE;
  virtual void AssignSysmemBuffers(
      const std::vector<media::SysmemBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id);
  virtual bool Flush() OVERRIDE;
  virtual bool Abort() OVERRIDE;

  // VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(
      uint32 requested_num_of_buffers,
      const gfx::Size& dimensions,
      media::VideoDecodeAccelerator::MemoryType type) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void NotifyEndOfStream() OVERRIDE;
  virtual void NotifyError(
      media::VideoDecodeAccelerator::Error error) OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 bitstream_buffer_id) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyAbortDone() OVERRIDE;

 private:
  void InitializeDecoder(const std::vector<uint32>& configs);

  // Client lifetime must exceed lifetime of this class.
  media::VideoDecodeAccelerator::Client* client_;

  // Host for GpuVideoDecodeAccelerator.
  scoped_ptr<media::VideoDecodeAccelerator> decoder_;

  // Host for Gpu Channel.
  scoped_refptr<GpuChannelHost> channel_;

  DISALLOW_COPY_AND_ASSIGN(PlatformVideoDecoderImpl);
};

// PlatformVideoDecoderImpl must extend RefCountedThreadSafe in order to post
// tasks on the IO loop. However, it is not actually ref counted:
// PPB_VideoDecode_Impl is the only thing that holds reference to
// PlatformVideoDecoderImpl, so ref counting is unnecessary.
//
// TODO(vrk): Not sure if this is the right thing to do. Talk with fischman.
DISABLE_RUNNABLE_METHOD_REFCOUNT(PlatformVideoDecoderImpl);

#endif  // CONTENT_RENDERER_PEPPER_PLATFORM_VIDEO_DECODER_IMPL_H_
