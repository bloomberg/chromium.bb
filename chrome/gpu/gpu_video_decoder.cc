// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_video_decoder.h"

#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_channel.h"
#include "chrome/gpu/media/fake_gl_video_decode_engine.h"
#include "media/base/data_buffer.h"
#include "media/base/video_frame.h"

void GpuVideoDecoder::OnChannelConnected(int32 peer_pid) {
}

void GpuVideoDecoder::OnChannelError() {
}

void GpuVideoDecoder::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecoder, msg)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_Initialize,
                        OnInitialize)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_Destroy,
                        OnUninitialize)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_Flush,
                        OnFlush)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_EmptyThisBuffer,
                        OnEmptyThisBuffer)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_FillThisBuffer,
                        OnFillThisBuffer)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderMsg_FillThisBufferDoneACK,
                        OnFillThisBufferDoneACK)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

bool GpuVideoDecoder::CreateInputTransferBuffer(
    uint32 size,
    base::SharedMemoryHandle* handle) {
  input_transfer_buffer_.reset(new base::SharedMemory);
  if (!input_transfer_buffer_.get())
    return false;

  if (!input_transfer_buffer_->Create(std::wstring(), false, false, size))
    return false;

  if (!input_transfer_buffer_->Map(size))
    return false;

  if (!input_transfer_buffer_->ShareToProcess(renderer_handle_, handle))
    return false;

  return true;
}

bool GpuVideoDecoder::CreateOutputTransferBuffer(
    uint32 size,
    base::SharedMemoryHandle* handle) {
  output_transfer_buffer_.reset(new base::SharedMemory);
  if (!output_transfer_buffer_.get())
    return false;

  if (!output_transfer_buffer_->Create(std::wstring(), false, false, size))
    return false;

  if (!output_transfer_buffer_->Map(size))
    return false;

  if (!output_transfer_buffer_->ShareToProcess(renderer_handle_, handle))
    return false;

  return true;
}

void GpuVideoDecoder::CreateVideoFrameOnTransferBuffer() {
  const base::TimeDelta kZero;
  uint8* data[media::VideoFrame::kMaxPlanes];
  int32 strides[media::VideoFrame::kMaxPlanes];
  memset(data, 0, sizeof(data));
  memset(strides, 0, sizeof(strides));
  data[0] = static_cast<uint8*>(output_transfer_buffer_->memory());
  data[1] = data[0] + config_.width * config_.height;
  data[2] = data[1] + config_.width * config_.height / 4;
  strides[0] = config_.width;
  strides[1] = strides[2] = config_.width >> 1;
  media::VideoFrame:: CreateFrameExternal(
      media::VideoFrame::TYPE_SYSTEM_MEMORY,
      media::VideoFrame::YV12,
      config_.width, config_.height, 3,
      data, strides,
      kZero, kZero,
      NULL,
      &frame_);
}

void GpuVideoDecoder::OnInitializeComplete(const VideoCodecInfo& info) {
  info_ = info;
  GpuVideoDecoderInitDoneParam param;
  param.success = false;
  param.input_buffer_handle = base::SharedMemory::NULLHandle();
  param.output_buffer_handle = base::SharedMemory::NULLHandle();

  if (!info.success) {
    SendInitializeDone(param);
    return;
  }

  // Translate surface type.
  param.surface_type = static_cast<GpuVideoDecoderInitDoneParam::SurfaceType>(
      info.stream_info.surface_type);

  // Translate surface format.
  switch (info.stream_info.surface_format) {
    case VideoFrame::YV12:
      param.format = GpuVideoDecoderInitDoneParam::SurfaceFormat_YV12;
      break;
    case VideoFrame::RGBA:
      param.format = GpuVideoDecoderInitDoneParam::SurfaceFormat_RGBA;
    default:
      NOTREACHED();
  }

  // TODO(jiesun): Check the assumption of input size < original size.
  param.input_buffer_size = config_.width * config_.height * 3 / 2;
  if (!CreateInputTransferBuffer(param.input_buffer_size,
                                 &param.input_buffer_handle)) {
    SendInitializeDone(param);
    return;
  }

  if (info.stream_info.surface_type == VideoFrame::TYPE_SYSTEM_MEMORY) {
    // TODO(jiesun): Allocate this according to the surface format.
    // The format actually could change during streaming, we need to
    // notify GpuVideoDecoderHost side when this happened and renegotiate
    // the transfer buffer.
    switch (info.stream_info.surface_format) {
      case VideoFrame::YV12:
        // TODO(jiesun): take stride into account.
        param.output_buffer_size =
            config_.width * config_.height * 3 / 2;
        break;
      default:
        NOTREACHED();
    }

    if (!CreateOutputTransferBuffer(param.output_buffer_size,
                                    &param.output_buffer_handle)) {
      SendInitializeDone(param);
      return;
    }
    CreateVideoFrameOnTransferBuffer();
  }

  param.success = true;

  SendInitializeDone(param);
}

void GpuVideoDecoder::OnUninitializeComplete() {
  SendUninitializeDone();
}

void GpuVideoDecoder::OnFlushComplete() {
  SendFlushDone();
}

void GpuVideoDecoder::OnSeekComplete() {
  NOTIMPLEMENTED();
}

void GpuVideoDecoder::OnError() {
  NOTIMPLEMENTED();
}

void GpuVideoDecoder::OnFormatChange(VideoStreamInfo stream_info) {
  NOTIMPLEMENTED();
}

void GpuVideoDecoder::ProduceVideoSample(scoped_refptr<Buffer> buffer) {
  SendEmptyBufferDone();
}

void GpuVideoDecoder::ConsumeVideoFrame(scoped_refptr<VideoFrame> frame) {
  GpuVideoDecoderOutputBufferParam output_param;
  output_param.timestamp = frame->GetTimestamp().InMicroseconds();
  output_param.duration = frame->GetDuration().InMicroseconds();
  output_param.flags = frame->IsEndOfStream() ?
      GpuVideoDecoderOutputBufferParam::kFlagsEndOfStream : 0;
  // TODO(hclam): We should have the conversion between VideoFrame and the
  // IPC transport param done in GpuVideoDevice.
  // This is a hack to pass texture back as a param.
  output_param.texture = frame->gl_texture(media::VideoFrame::kRGBPlane);
  SendFillBufferDone(output_param);
}

void* GpuVideoDecoder::GetDevice() {
  // Simply delegate the method call to GpuVideoDevice.
  return decode_context_->GetDevice();
}

void GpuVideoDecoder::AllocateVideoFrames(
    int n, size_t width, size_t height,
    AllocationCompleteCallback* callback) {
  // Since the communication between Renderer and GPU process is by GL textures.
  // We need to obtain a set of GL textures by sending IPC commands to the
  // Renderer process. The recipient of these commands will be IpcVideoDecoder.
  //
  // After IpcVideoDecoder replied with a set of textures. We'll assign these
  // textures to GpuVideoDevice. They will be used to generate platform
  // specific VideoFrames objects that are used by VideoDecodeEngine.
  //
  // After GL textures are assigned we'll proceed with allocation the
  // VideoFrames. GpuVideoDevice::CreateVideoFramesFromGlTextures() will be
  // called.
  //
  // When GpuVideoDevice replied with a set of VideoFrames we'll give
  // that to VideoDecodeEngine and the cycle of video frame allocation is done.
  //
  // Note that this method is called when there's no video frames allocated or
  // they were all released.
}

void GpuVideoDecoder::ReleaseVideoFrames(int n, VideoFrame* frames) {
  // This method will first call to GpuVideoDevice to release all the resource
  // associated with a VideoFrame.
  //
  // And when we'll call GpuVideoDevice::ReleaseVideoFrames to remove the set
  // of Gl textures associated with the context.
  //
  // And finally we'll send IPC commands to IpcVideoDecoder to destroy all
  // GL textures generated.
}

void GpuVideoDecoder::Destroy(DestructionCompleteCallback* callback) {
  //  TODO(hclam): I still need to think what I should do here.
}

GpuVideoDecoder::GpuVideoDecoder(
    const GpuVideoDecoderInfoParam* param,
    GpuChannel* channel,
    base::ProcessHandle handle,
    gpu::gles2::GLES2Decoder* decoder)
    : decoder_host_route_id_(param->decoder_host_route_id),
      output_transfer_buffer_busy_(false),
      pending_output_requests_(0),
      channel_(channel),
      renderer_handle_(handle),
      gles2_decoder_(decoder) {
  memset(&config_, 0, sizeof(config_));
  memset(&info_, 0, sizeof(info_));

  // TODO(jiesun): find a better way to determine which VideoDecodeEngine
  // to return on current platform.
  decode_engine_.reset(new FakeGlVideoDecodeEngine());
}

void GpuVideoDecoder::OnInitialize(const GpuVideoDecoderInitParam& param) {
  // TODO(hclam): Initialize the VideoDecodeContext first.

  // TODO(jiesun): codec id should come from |param|.
  config_.codec = media::kCodecH264;
  config_.width = param.width;
  config_.height = param.height;
  config_.opaque_context = NULL;
  decode_engine_->Initialize(NULL, this, config_);
}

void GpuVideoDecoder::OnUninitialize() {
  decode_engine_->Uninitialize();
}

void GpuVideoDecoder::OnFlush() {
  // TODO(jiesun): this is wrong??
  output_transfer_buffer_busy_ = false;
  pending_output_requests_ = 0;

  decode_engine_->Flush();
}

void GpuVideoDecoder::OnEmptyThisBuffer(
    const GpuVideoDecoderInputBufferParam& buffer) {
  DCHECK(input_transfer_buffer_->memory());

  uint8* src = static_cast<uint8*>(input_transfer_buffer_->memory());

  scoped_refptr<Buffer> input_buffer;
  uint8* dst = buffer.size ? new uint8[buffer.size] : NULL;
  input_buffer = new media::DataBuffer(dst, buffer.size);
  memcpy(dst, src, buffer.size);
  SendEmptyBufferACK();

  decode_engine_->ConsumeVideoSample(input_buffer);
}

void GpuVideoDecoder::OnFillThisBuffer(
    const GpuVideoDecoderOutputBufferParam& param) {
  // Switch context before calling to the decode engine.
  // TODO(hclam): This is temporary to allow FakeGlVideoDecodeEngine to issue
  // GL commands correctly.
  bool ret = gles2_decoder_->MakeCurrent();
  DCHECK(ret) << "Failed to switch context";

  if (info_.stream_info.surface_type == VideoFrame::TYPE_SYSTEM_MEMORY) {
    pending_output_requests_++;
    if (!output_transfer_buffer_busy_) {
      output_transfer_buffer_busy_ = true;
      decode_engine_->ProduceVideoFrame(frame_);
    }
  } else {
    // TODO(hclam): I need to rethink how to delegate calls to
    // VideoDecodeEngine, I may need to create a GpuVideoDecodeContext that
    // provides a method for me to make calls to VideoDecodeEngine with the
    // correct VideoFrame.
    DCHECK_EQ(VideoFrame::TYPE_GL_TEXTURE, info_.stream_info.surface_type);

    scoped_refptr<media::VideoFrame> frame;
    VideoFrame::GlTexture textures[3] = { param.texture, 0, 0 };

    media::VideoFrame:: CreateFrameGlTexture(
        media::VideoFrame::RGBA, config_.width, config_.height, textures,
        base::TimeDelta(), base::TimeDelta(), &frame);
    decode_engine_->ProduceVideoFrame(frame);
  }
}

void GpuVideoDecoder::OnFillThisBufferDoneACK() {
  if (info_.stream_info.surface_type == VideoFrame::TYPE_SYSTEM_MEMORY) {
    output_transfer_buffer_busy_ = false;
    pending_output_requests_--;
    if (pending_output_requests_) {
      output_transfer_buffer_busy_ = true;
      decode_engine_->ProduceVideoFrame(frame_);
    }
  }
}

void GpuVideoDecoder::SendInitializeDone(
    const GpuVideoDecoderInitDoneParam& param) {
  if (!channel_->Send(
      new GpuVideoDecoderHostMsg_InitializeACK(route_id(), param))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_InitializeACK failed";
  }
}

void GpuVideoDecoder::SendUninitializeDone() {
  if (!channel_->Send(new GpuVideoDecoderHostMsg_DestroyACK(route_id()))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_DestroyACK failed";
  }
}

void GpuVideoDecoder::SendFlushDone() {
  if (!channel_->Send(new GpuVideoDecoderHostMsg_FlushACK(route_id()))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_FlushACK failed";
  }
}

void GpuVideoDecoder::SendEmptyBufferDone() {
  if (!channel_->Send(
      new GpuVideoDecoderHostMsg_EmptyThisBufferDone(route_id()))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_EmptyThisBufferDone failed";
  }
}

void GpuVideoDecoder::SendEmptyBufferACK() {
  if (!channel_->Send(
      new GpuVideoDecoderHostMsg_EmptyThisBufferACK(route_id()))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_EmptyThisBufferACK failed";
  }
}

void GpuVideoDecoder::SendFillBufferDone(
    const GpuVideoDecoderOutputBufferParam& param) {
  if (!channel_->Send(
      new GpuVideoDecoderHostMsg_FillThisBufferDone(route_id(), param))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_FillThisBufferDone failed";
  }
}
