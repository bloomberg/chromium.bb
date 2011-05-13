// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper_platform_video_decoder_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "content/common/child_process.h"
#include "content/renderer/gpu_channel_host.h"
#include "content/renderer/gpu_video_decode_accelerator_host.h"
#include "content/renderer/gpu_video_service_host.h"
#include "content/renderer/render_thread.h"

using media::BitstreamBuffer;
using media::VideoDecodeAcceleratorCallback;

PlatformVideoDecoderImpl::PlatformVideoDecoderImpl(
    VideoDecodeAccelerator::Client* client)
    : client_(client),
      decoder_(NULL) {
  DCHECK(client);
}

PlatformVideoDecoderImpl::~PlatformVideoDecoderImpl() {}

void PlatformVideoDecoderImpl::GetConfigs(
    const std::vector<uint32>& requested_configs,
    std::vector<uint32>* matched_configs) {
  // TODO(vrk): Implement.
  NOTIMPLEMENTED();
}

bool PlatformVideoDecoderImpl::Initialize(const std::vector<uint32>& config) {
  // TODO(vrk): Support multiple decoders.
  if (decoder_.get())
    return true;

  RenderThread* render_thread = RenderThread::current();
  DCHECK(render_thread);

  channel_ = render_thread->EstablishGpuChannelSync(
      content::CAUSE_FOR_GPU_LAUNCH_VIDEODECODEACCELERATOR_INITIALIZE);

  if (!channel_.get())
    return false;

  DCHECK_EQ(channel_->state(), GpuChannelHost::kConnected);

  // Set a callback to ensure decoder is only initialized after channel is
  // connected and GpuVidoServiceHost message filter is added to channel.
  //
  // TODO(vrk): Initialize should take a callback to be called (on the
  // renderer's thread) when initialization is completed.
  base::Closure initialize = base::Bind(
      &PlatformVideoDecoderImpl::InitializeDecoder,
      base::Unretained(this),
      config);

  GpuVideoServiceHost* video_service = channel_->gpu_video_service_host();
  video_service->SetOnInitialized(initialize);
  return true;
}

void PlatformVideoDecoderImpl::InitializeDecoder(
    const std::vector<uint32>& configs) {
  // Only create GpuVideoDecodeAcceleratorHost on IO thread.
  if (ChildProcess::current()->io_message_loop() != MessageLoop::current() ) {
    ChildProcess::current()->io_message_loop()->
        PostTask(FROM_HERE, base::Bind(
            &PlatformVideoDecoderImpl::InitializeDecoder,
            base::Unretained(this),
            configs));
    return;
  }
  GpuVideoServiceHost* video_service = channel_->gpu_video_service_host();
  decoder_.reset(video_service->CreateVideoAccelerator(this));

  // Send IPC message to initialize decoder in GPU process.
  decoder_->Initialize(configs);
}

bool PlatformVideoDecoderImpl::Decode(const BitstreamBuffer& bitstream_buffer) {
  DCHECK(decoder_.get());
  return decoder_->Decode(bitstream_buffer);
}

void PlatformVideoDecoderImpl::AssignGLESBuffers(
    const std::vector<media::GLESBuffer>& buffers) {
  DCHECK(decoder_.get());
  decoder_->AssignGLESBuffers(buffers);
}

void PlatformVideoDecoderImpl::AssignSysmemBuffers(
    const std::vector<media::SysmemBuffer>& buffers) {
  DCHECK(decoder_.get());
  decoder_->AssignSysmemBuffers(buffers);
}

void PlatformVideoDecoderImpl::ReusePictureBuffer(
    int32 picture_buffer_id) {
  DCHECK(decoder_.get());
  decoder_->ReusePictureBuffer(picture_buffer_id);
}

bool PlatformVideoDecoderImpl::Flush() {
  DCHECK(decoder_.get());
  return decoder_->Flush();
}

bool PlatformVideoDecoderImpl::Abort() {
  DCHECK(decoder_.get());
  return decoder_->Abort();
}

void PlatformVideoDecoderImpl::NotifyEndOfStream() {
  client_->NotifyEndOfStream();
}

void PlatformVideoDecoderImpl::NotifyError(
    VideoDecodeAccelerator::Error error) {
  client_->NotifyError(error);
}

void PlatformVideoDecoderImpl::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    media::VideoDecodeAccelerator::MemoryType type) {
  client_->ProvidePictureBuffers(requested_num_of_buffers, dimensions, type);
}

void PlatformVideoDecoderImpl::DismissPictureBuffer(int32 picture_buffer_id) {
  client_->DismissPictureBuffer(picture_buffer_id);
}

void PlatformVideoDecoderImpl::PictureReady(const media::Picture& picture) {
  client_->PictureReady(picture);
}

void PlatformVideoDecoderImpl::NotifyEndOfBitstreamBuffer(
    int32 bitstream_buffer_id) {
  client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void PlatformVideoDecoderImpl::NotifyFlushDone() {
  client_->NotifyFlushDone();
}

void PlatformVideoDecoderImpl::NotifyAbortDone() {
  client_->NotifyAbortDone();
}
