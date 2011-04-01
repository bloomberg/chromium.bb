// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper_platform_video_decoder_impl.h"

#include <vector>

#include "base/logging.h"
#include "content/renderer/video_decode_accelerator_host.h"
#include "media/base/buffers.h"
#include "media/base/video_frame.h"

using media::BitstreamBuffer;
using media::VideoDecodeAccelerator;
using media::VideoDecodeAcceleratorCallback;

PlatformVideoDecoderImpl::PlatformVideoDecoderImpl(
    VideoDecodeAccelerator* video_decode_accelerator)
    : client_(NULL),
      video_decode_accelerator_(video_decode_accelerator) {
}

PlatformVideoDecoderImpl::~PlatformVideoDecoderImpl() {}

const std::vector<uint32>& PlatformVideoDecoderImpl::GetConfig(
    const std::vector<uint32>& prototype_config) {
  NOTIMPLEMENTED();
  return configs;
}

bool PlatformVideoDecoderImpl::Initialize(const std::vector<uint32>& config) {
  // TODO(vmr): Create video decoder in GPU process.

  return true;
}

bool PlatformVideoDecoderImpl::Decode(
    BitstreamBuffer* bitstream_buffer,
    VideoDecodeAcceleratorCallback* callback) {
  // TODO(vmr): Implement me!
  NOTIMPLEMENTED();
  // Put the incoming buffer into bitstream_buffer queue
  return false;
}

void PlatformVideoDecoderImpl::AssignPictureBuffer(
    std::vector<VideoDecodeAccelerator::PictureBuffer*> picture_buffers) {
}

void PlatformVideoDecoderImpl::ReusePictureBuffer(
    VideoDecodeAccelerator::PictureBuffer* picture_buffer) {
}

bool PlatformVideoDecoderImpl::Flush(
    VideoDecodeAcceleratorCallback* callback) {
  // TODO(vmr): Implement me!
  NOTIMPLEMENTED();

  // Call GPU video decoder to flush.
  return video_decode_accelerator_->Flush(callback);
}

bool PlatformVideoDecoderImpl::Abort(
    VideoDecodeAcceleratorCallback* callback) {
  // TODO(vmr): Implement me!
  NOTIMPLEMENTED();

  // Call GPU video decoder to abort.
  return video_decode_accelerator_->Abort(callback);
}

void PlatformVideoDecoderImpl::NotifyEndOfStream() {
  // TODO(vmr): Implement.
  NOTIMPLEMENTED();
}

void PlatformVideoDecoderImpl::NotifyError(
    VideoDecodeAccelerator::Error error) {
  // TODO(vmr): Implement.
  NOTIMPLEMENTED();
}

void PlatformVideoDecoderImpl::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const std::vector<uint32>& buffer_properties) {
  // TODO(vmr): Implement.
  NOTIMPLEMENTED();
}

void PlatformVideoDecoderImpl::PictureReady(
    VideoDecodeAccelerator::Picture* picture) {
  // TODO(vmr): Implement.
  NOTIMPLEMENTED();
}
