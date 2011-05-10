// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper_platform_video_decoder_impl.h"

#include <vector>

#include "base/logging.h"
#include "content/renderer/video_decode_accelerator_host.h"

using media::BitstreamBuffer;
using media::VideoDecodeAcceleratorCallback;

PlatformVideoDecoderImpl::PlatformVideoDecoderImpl(
    VideoDecodeAccelerator* video_decode_accelerator)
    : client_(NULL),
      video_decode_accelerator_(video_decode_accelerator) {
}

PlatformVideoDecoderImpl::~PlatformVideoDecoderImpl() {}

void PlatformVideoDecoderImpl::GetConfigs(
    const std::vector<uint32>& requested_configs,
    std::vector<uint32>* matched_configs) {
  NOTIMPLEMENTED();
}

bool PlatformVideoDecoderImpl::Initialize(const std::vector<uint32>& config) {
  // TODO(vmr): Create video decoder in GPU process.

  return true;
}

bool PlatformVideoDecoderImpl::Decode(
    const BitstreamBuffer& bitstream_buffer,
    VideoDecodeAcceleratorCallback* callback) {
  // TODO(vmr): Implement me!
  NOTIMPLEMENTED();
  // Put the incoming buffer into bitstream_buffer queue
  return false;
}

void PlatformVideoDecoderImpl::AssignGLESBuffers(
    const std::vector<media::GLESBuffer>& buffers) {
  // TODO(vmr): Implement me!
  NOTIMPLEMENTED();
}

void PlatformVideoDecoderImpl::AssignSysmemBuffers(
    const std::vector<media::SysmemBuffer>& buffers) {
  // TODO(vmr): Implement me!
  NOTIMPLEMENTED();
}

void PlatformVideoDecoderImpl::ReusePictureBuffer(uint32 picture_buffer_id) {
  // TODO(vmr): Implement me!
  NOTIMPLEMENTED();
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
    gfx::Size dimensions,
    media::VideoDecodeAccelerator::MemoryType type) {
  // TODO(vmr): Implement.
  NOTIMPLEMENTED();
}

void PlatformVideoDecoderImpl::PictureReady(
    const media::Picture& picture) {
  // TODO(vmr): Implement.
  NOTIMPLEMENTED();
}
