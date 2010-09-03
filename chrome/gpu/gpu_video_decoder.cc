// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_video_decoder.h"

#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_channel.h"
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
  data[1] = data[0] + config_.width_ * config_.height_;
  data[2] = data[1] + config_.width_ * config_.height_ / 4;
  strides[0] = config_.width_;
  strides[1] = strides[2] = config_.width_ >> 1;
  media::VideoFrame:: CreateFrameExternal(
      media::VideoFrame::TYPE_SYSTEM_MEMORY,
      media::VideoFrame::YV12,
      config_.width_, config_.height_, 3,
      data, strides,
      kZero, kZero,
      NULL,
      &frame_);
}

void GpuVideoDecoder::OnInitializeComplete(const VideoCodecInfo& info) {
  info_ = info;
  GpuVideoDecoderInitDoneParam param;
  param.success_ = false;
  param.input_buffer_handle_ = base::SharedMemory::NULLHandle();
  param.output_buffer_handle_ = base::SharedMemory::NULLHandle();

  if (!info.success_) {
    SendInitializeDone(param);
    return;
  }

  // Translate surface type.
  switch (info.stream_info_.surface_type_) {
    case VideoFrame::TYPE_SYSTEM_MEMORY:
      param.surface_type_ =
          GpuVideoDecoderInitDoneParam::SurfaceTypeSystemMemory;
      break;
    case VideoFrame::TYPE_DIRECT3DSURFACE:
    case VideoFrame::TYPE_EGL_IMAGE:
    default:
      NOTREACHED();
  }

  // Translate surface format.
  switch (info.stream_info_.surface_format_) {
    case VideoFrame::YV12:
      param.format_ = GpuVideoDecoderInitDoneParam::SurfaceFormat_YV12;
      break;
    default:
      NOTREACHED();
  }

  // TODO(jiesun): Check the assumption of input size < original size.
  param.input_buffer_size_ = config_.width_ * config_.height_ * 3 / 2;
  if (!CreateInputTransferBuffer(param.input_buffer_size_,
                                 &param.input_buffer_handle_)) {
    SendInitializeDone(param);
    return;
  }

  if (info.stream_info_.surface_type_ == VideoFrame::TYPE_SYSTEM_MEMORY) {
    // TODO(jiesun): Allocate this according to the surface format.
    // The format actually could change during streaming, we need to
    // notify GpuVideoDecoderHost side when this happened and renegotiate
    // the transfer buffer.

    switch (info.stream_info_.surface_format_) {
      case VideoFrame::YV12:
        // TODO(jiesun): take stride into account.
        param.output_buffer_size_ =
            config_.width_ * config_.height_ * 3 / 2;
        break;
      default:
        NOTREACHED();
    }

    if (!CreateOutputTransferBuffer(param.output_buffer_size_,
                                    &param.output_buffer_handle_)) {
      SendInitializeDone(param);
      return;
    }

    CreateVideoFrameOnTransferBuffer();
  }

  param.success_ = true;

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

void GpuVideoDecoder::OnEmptyBufferCallback(scoped_refptr<Buffer> buffer) {
  SendEmptyBufferDone();
}

void GpuVideoDecoder::OnFillBufferCallback(scoped_refptr<VideoFrame> frame) {
  GpuVideoDecoderOutputBufferParam output_param;
  output_param.timestamp_ = frame->GetTimestamp().InMicroseconds();
  output_param.duration_ = frame->GetDuration().InMicroseconds();
  output_param.flags_ = frame->IsEndOfStream() ?
      GpuVideoDecoderOutputBufferParam::kFlagsEndOfStream : 0;
  SendFillBufferDone(output_param);
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
#if defined(OS_WIN) && defined(ENABLE_GPU_DECODER)
  // TODO(jiesun): find a better way to determine which GpuVideoDecoder
  // to return on current platform.
  decode_engine_.reset(new GpuVideoDecoderMFT());
#else
#endif
}

void GpuVideoDecoder::OnInitialize(const GpuVideoDecoderInitParam& param) {
  // TODO(jiesun): codec id should come from |param|.
  config_.codec_ = media::kCodecH264;
  config_.width_ = param.width_;
  config_.height_ = param.height_;
  config_.opaque_context_ = NULL;
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
  uint8* dst = buffer.size_ ? new uint8[buffer.size_] : NULL;
  input_buffer = new media::DataBuffer(dst, buffer.size_);
  memcpy(dst, src, buffer.size_);
  SendEmptyBufferACK();

  decode_engine_->EmptyThisBuffer(input_buffer);
}
void GpuVideoDecoder::OnFillThisBuffer(
    const GpuVideoDecoderOutputBufferParam& frame) {
  if (info_.stream_info_.surface_type_ == VideoFrame::TYPE_SYSTEM_MEMORY) {
    pending_output_requests_++;
    if (!output_transfer_buffer_busy_) {
      output_transfer_buffer_busy_ = true;
      decode_engine_->FillThisBuffer(frame_);
    }
  } else {
    decode_engine_->FillThisBuffer(frame_);
  }
}

void GpuVideoDecoder::OnFillThisBufferDoneACK() {
  if (info_.stream_info_.surface_type_ == VideoFrame::TYPE_SYSTEM_MEMORY) {
    output_transfer_buffer_busy_ = false;
    pending_output_requests_--;
    if (pending_output_requests_) {
      output_transfer_buffer_busy_ = true;
      decode_engine_->FillThisBuffer(frame_);
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
    const GpuVideoDecoderOutputBufferParam& frame) {
  if (!channel_->Send(
      new GpuVideoDecoderHostMsg_FillThisBufferDone(route_id(), frame))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_FillThisBufferDone failed";
  }
}
