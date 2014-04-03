// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/pepper_platform_video_decoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/child/child_process.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/renderer/render_thread_impl.h"

using media::BitstreamBuffer;

namespace content {

PlatformVideoDecoder::PlatformVideoDecoder(int32 command_buffer_route_id)
    : command_buffer_route_id_(command_buffer_route_id) {}

PlatformVideoDecoder::~PlatformVideoDecoder() {}

bool PlatformVideoDecoder::Initialize(
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  client_ = client;

  // TODO(vrk): Support multiple decoders.
  if (decoder_)
    return true;

  RenderThreadImpl* render_thread = RenderThreadImpl::current();

  // This is not synchronous, but subsequent IPC messages will be buffered, so
  // it is okay to immediately send IPC messages through the returned channel.
  GpuChannelHost* channel =
      render_thread->EstablishGpuChannelSync(
          CAUSE_FOR_GPU_LAUNCH_VIDEODECODEACCELERATOR_INITIALIZE);

  if (!channel)
    return false;

  // Send IPC message to initialize decoder in GPU process.
  decoder_ = channel->CreateVideoDecoder(command_buffer_route_id_);
  return (decoder_ && decoder_->Initialize(profile, this));
}

void PlatformVideoDecoder::Decode(const BitstreamBuffer& bitstream_buffer) {
  DCHECK(decoder_.get());
  decoder_->Decode(bitstream_buffer);
}

void PlatformVideoDecoder::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(decoder_.get());
  decoder_->AssignPictureBuffers(buffers);
}

void PlatformVideoDecoder::ReusePictureBuffer(int32 picture_buffer_id) {
  DCHECK(decoder_.get());
  decoder_->ReusePictureBuffer(picture_buffer_id);
}

void PlatformVideoDecoder::Flush() {
  DCHECK(decoder_.get());
  decoder_->Flush();
}

void PlatformVideoDecoder::Reset() {
  DCHECK(decoder_.get());
  decoder_->Reset();
}

void PlatformVideoDecoder::Destroy() {
  if (decoder_)
    decoder_.release()->Destroy();
  client_ = NULL;
  delete this;
}

void PlatformVideoDecoder::NotifyError(
    VideoDecodeAccelerator::Error error) {
  DCHECK(RenderThreadImpl::current());
  client_->NotifyError(error);
}

void PlatformVideoDecoder::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    uint32 texture_target) {
  DCHECK(RenderThreadImpl::current());
  client_->ProvidePictureBuffers(requested_num_of_buffers, dimensions,
                                 texture_target);
}

void PlatformVideoDecoder::DismissPictureBuffer(int32 picture_buffer_id) {
  DCHECK(RenderThreadImpl::current());
  client_->DismissPictureBuffer(picture_buffer_id);
}

void PlatformVideoDecoder::PictureReady(const media::Picture& picture) {
  DCHECK(RenderThreadImpl::current());
  client_->PictureReady(picture);
}

void PlatformVideoDecoder::NotifyEndOfBitstreamBuffer(
  int32 bitstream_buffer_id) {
  DCHECK(RenderThreadImpl::current());
  client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void PlatformVideoDecoder::NotifyFlushDone() {
  DCHECK(RenderThreadImpl::current());
  client_->NotifyFlushDone();
}

void PlatformVideoDecoder::NotifyResetDone() {
  DCHECK(RenderThreadImpl::current());
  client_->NotifyResetDone();
}

}  // namespace content
