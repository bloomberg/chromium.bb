// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper_platform_video_decoder_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "content/common/child_process.h"
#include "content/renderer/gpu/gpu_channel_host.h"
#include "content/renderer/gpu/gpu_video_decode_accelerator_host.h"
#include "content/renderer/gpu/gpu_video_service_host.h"
#include "content/renderer/render_thread.h"

using media::BitstreamBuffer;

PlatformVideoDecoderImpl::PlatformVideoDecoderImpl(
    VideoDecodeAccelerator::Client* client, uint32 command_buffer_route_id)
    : client_(client),
      command_buffer_route_id_(command_buffer_route_id),
      decoder_(NULL),
      message_loop_(NULL) {
  DCHECK(client);
}

PlatformVideoDecoderImpl::~PlatformVideoDecoderImpl() {}

bool PlatformVideoDecoderImpl::GetConfigs(
    const std::vector<uint32>& requested_configs,
    std::vector<uint32>* matched_configs) {
  // TODO(vrk): Implement.
  NOTIMPLEMENTED();
  return true;
}

bool PlatformVideoDecoderImpl::Initialize(const std::vector<uint32>& config) {
  // TODO(vrk): Support multiple decoders.
  if (decoder_.get())
    return true;

  RenderThread* render_thread = RenderThread::current();
  DCHECK(render_thread);
  message_loop_ = MessageLoop::current();
  DCHECK(message_loop_);

  channel_ = render_thread->EstablishGpuChannelSync(
      content::CAUSE_FOR_GPU_LAUNCH_VIDEODECODEACCELERATOR_INITIALIZE);

  if (!channel_.get())
    return false;

  DCHECK_EQ(channel_->state(), GpuChannelHost::kConnected);

  // Set a callback to ensure decoder is only initialized after channel is
  // connected and GpuVidoServiceHost message filter is added to channel.
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
  decoder_.reset(video_service->CreateVideoAccelerator(
      this, command_buffer_route_id_));

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
  DCHECK(message_loop_);
  message_loop_->
      PostTask(FROM_HERE, base::Bind(
          &VideoDecodeAccelerator::Client::NotifyEndOfStream,
          base::Unretained(client_)));
}

void PlatformVideoDecoderImpl::NotifyError(
    VideoDecodeAccelerator::Error error) {
  DCHECK(message_loop_);
  message_loop_->
      PostTask(FROM_HERE, base::Bind(
          &VideoDecodeAccelerator::Client::NotifyError,
          base::Unretained(client_),
          error));
}

void PlatformVideoDecoderImpl::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const gfx::Size& dimensions,
    media::VideoDecodeAccelerator::MemoryType type) {
  DCHECK(message_loop_);
  message_loop_->
      PostTask(FROM_HERE, base::Bind(
          &VideoDecodeAccelerator::Client::ProvidePictureBuffers,
          base::Unretained(client_),
          requested_num_of_buffers,
          dimensions,
          type));
}

void PlatformVideoDecoderImpl::DismissPictureBuffer(int32 picture_buffer_id) {
  DCHECK(message_loop_);
  message_loop_->
      PostTask(FROM_HERE, base::Bind(
          &VideoDecodeAccelerator::Client::DismissPictureBuffer,
          base::Unretained(client_),
          picture_buffer_id));
}

void PlatformVideoDecoderImpl::PictureReady(const media::Picture& picture) {
  DCHECK(message_loop_);
  message_loop_->
      PostTask(FROM_HERE, base::Bind(
          &VideoDecodeAccelerator::Client::PictureReady,
          base::Unretained(client_),
          picture));
}

void PlatformVideoDecoderImpl::NotifyInitializeDone() {
  DCHECK(message_loop_);
  message_loop_->
      PostTask(FROM_HERE, base::Bind(
          &VideoDecodeAccelerator::Client::NotifyInitializeDone,
          base::Unretained(client_)));
}

void PlatformVideoDecoderImpl::NotifyEndOfBitstreamBuffer(
  int32 bitstream_buffer_id) {
  DCHECK(message_loop_);
  message_loop_->
      PostTask(FROM_HERE, base::Bind(
          &VideoDecodeAccelerator::Client::NotifyEndOfBitstreamBuffer,
          base::Unretained(client_),
          bitstream_buffer_id));
}

void PlatformVideoDecoderImpl::NotifyFlushDone() {
  DCHECK(message_loop_);
  message_loop_->
      PostTask(FROM_HERE, base::Bind(
          &VideoDecodeAccelerator::Client::NotifyFlushDone,
          base::Unretained(client_)));
}

void PlatformVideoDecoderImpl::NotifyAbortDone() {
  DCHECK(message_loop_);
  message_loop_->
      PostTask(FROM_HERE, base::Bind(
          &VideoDecodeAccelerator::Client::NotifyAbortDone,
          base::Unretained(client_)));
}
