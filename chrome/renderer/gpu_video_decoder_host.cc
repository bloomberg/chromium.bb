// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/gpu_video_decoder_host.h"

#include "chrome/common/gpu_messages.h"
#include "chrome/renderer/gpu_video_service_host.h"
#include "chrome/renderer/render_thread.h"

GpuVideoDecoderHost::GpuVideoDecoderHost(GpuVideoServiceHost* service_host,
                                         GpuChannelHost* channel_host,
                                         int context_route_id)
    : gpu_video_service_host_(service_host),
      channel_host_(channel_host),
      context_route_id_(context_route_id),
      event_handler_(NULL),
      buffer_id_serial_(0),
      state_(kStateUninitialized),
      input_buffer_busy_(false) {
  memset(&init_param_, 0, sizeof(init_param_));
  memset(&done_param_, 0, sizeof(done_param_));
}

void GpuVideoDecoderHost::OnChannelError() {
  channel_host_ = NULL;
}

void GpuVideoDecoderHost::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecoderHost, msg)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_InitializeACK,
                        OnInitializeDone)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_DestroyACK,
                        OnUninitializeDone)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_FlushACK,
                        OnFlushDone)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_EmptyThisBufferACK,
                        OnEmptyThisBufferACK)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_EmptyThisBufferDone,
                        OnEmptyThisBufferDone)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_FillThisBufferDone,
                        OnFillThisBufferDone)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

bool GpuVideoDecoderHost::Initialize(EventHandler* event_handler,
                                     const GpuVideoDecoderInitParam& param) {
  DCHECK_EQ(state_, kStateUninitialized);

  // Save the event handler before we perform initialization operations so
  // that we can report initialization events.
  event_handler_ = event_handler;

  // TODO(hclam): Pass the context route ID here.
  // TODO(hclam): This create video decoder operation is synchronous, need to
  // make it asynchronous.
  decoder_info_.context_id = context_route_id_;
  if (!channel_host_->Send(
          new GpuChannelMsg_CreateVideoDecoder(&decoder_info_))) {
    LOG(ERROR) << "GpuChannelMsg_CreateVideoDecoder failed";
    return false;
  }

  // Add the route so we'll receive messages.
  gpu_video_service_host_->AddRoute(my_route_id(), this);

  init_param_ = param;
  if (!channel_host_ || !channel_host_->Send(
      new GpuVideoDecoderMsg_Initialize(route_id(), param))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_Initialize failed";
    return false;
  }
  return true;
}

bool GpuVideoDecoderHost::Uninitialize() {
  if (!channel_host_ || !channel_host_->Send(
      new GpuVideoDecoderMsg_Destroy(route_id()))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_Destroy failed";
    return false;
  }

  gpu_video_service_host_->RemoveRoute(my_route_id());
  return true;
}

void GpuVideoDecoderHost::EmptyThisBuffer(scoped_refptr<Buffer> buffer) {
  DCHECK_NE(state_, kStateUninitialized);
  DCHECK_NE(state_, kStateFlushing);

  // We never own input buffers, therefore when client in flush state, it
  // never call us with EmptyThisBuffer.
  if (state_ != kStateNormal)
    return;

  input_buffer_queue_.push_back(buffer);
  SendInputBufferToGpu();
}

void GpuVideoDecoderHost::FillThisBuffer(scoped_refptr<VideoFrame> frame) {
  DCHECK_NE(state_, kStateUninitialized);

  // Depends on who provides buffer. client could return buffer to
  // us while flushing.
  if (state_ == kStateError)
    return;

  GpuVideoDecoderOutputBufferParam param;
  if (!channel_host_ || !channel_host_->Send(
      new GpuVideoDecoderMsg_FillThisBuffer(route_id(), param))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_FillThisBuffer failed";
  }
}

bool GpuVideoDecoderHost::Flush() {
  state_ = kStateFlushing;
  if (!channel_host_ || !channel_host_->Send(
      new GpuVideoDecoderMsg_Flush(route_id()))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_Flush failed";
    return false;
  }
  input_buffer_queue_.clear();
  // TODO(jiesun): because GpuVideoDeocder/GpuVideoDecoder are asynchronously.
  // We need a way to make flush logic more clear. but I think ring buffer
  // should make the busy flag obsolete, therefore I will leave it for now.
  input_buffer_busy_ = false;
  return true;
}

void GpuVideoDecoderHost::OnInitializeDone(
    const GpuVideoDecoderInitDoneParam& param) {
  done_param_ = param;
  bool success = false;

  do {
    if (!param.success_)
      break;

    if (!base::SharedMemory::IsHandleValid(param.input_buffer_handle_))
      break;
    input_transfer_buffer_.reset(
        new base::SharedMemory(param.input_buffer_handle_, false));
    if (!input_transfer_buffer_->Map(param.input_buffer_size_))
      break;

    if (!base::SharedMemory::IsHandleValid(param.output_buffer_handle_))
      break;
    output_transfer_buffer_.reset(
        new base::SharedMemory(param.output_buffer_handle_, false));
    if (!output_transfer_buffer_->Map(param.output_buffer_size_))
      break;

    success = true;
  } while (0);

  state_ = success ? kStateNormal : kStateError;
  event_handler_->OnInitializeDone(success, param);
}

void GpuVideoDecoderHost::OnUninitializeDone() {
  input_transfer_buffer_.reset();
  output_transfer_buffer_.reset();

  event_handler_->OnUninitializeDone();
}

void GpuVideoDecoderHost::OnFlushDone() {
  state_ = kStateNormal;
  event_handler_->OnFlushDone();
}

void GpuVideoDecoderHost::OnEmptyThisBufferDone() {
  scoped_refptr<Buffer> buffer;
  event_handler_->OnEmptyBufferDone(buffer);
}

void GpuVideoDecoderHost::OnFillThisBufferDone(
    const GpuVideoDecoderOutputBufferParam& param) {
  scoped_refptr<VideoFrame> frame;

  if (param.flags_ & GpuVideoDecoderOutputBufferParam::kFlagsEndOfStream) {
    VideoFrame::CreateEmptyFrame(&frame);
  } else {
    VideoFrame::CreateFrame(VideoFrame::YV12,
                            init_param_.width_,
                            init_param_.height_,
                            base::TimeDelta::FromMicroseconds(param.timestamp_),
                            base::TimeDelta::FromMicroseconds(param.duration_),
                            &frame);

    uint8* src = static_cast<uint8*>(output_transfer_buffer_->memory());
    uint8* data0 = frame->data(0);
    uint8* data1 = frame->data(1);
    uint8* data2 = frame->data(2);
    int32 size = init_param_.width_ * init_param_.height_;
    memcpy(data0, src, size);
    memcpy(data1, src + size, size / 4);
    memcpy(data2, src + size + size / 4, size / 4);
  }

  event_handler_->OnFillBufferDone(frame);
  if (!channel_host_ || !channel_host_->Send(
      new GpuVideoDecoderMsg_FillThisBufferDoneACK(route_id()))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_FillThisBufferDoneACK failed";
  }
}

void GpuVideoDecoderHost::OnEmptyThisBufferACK() {
  input_buffer_busy_ = false;
  SendInputBufferToGpu();
}

void GpuVideoDecoderHost::SendInputBufferToGpu() {
  if (input_buffer_busy_) return;
  if (input_buffer_queue_.empty()) return;

  input_buffer_busy_ = true;

  scoped_refptr<Buffer> buffer;
  buffer = input_buffer_queue_.front();
  input_buffer_queue_.pop_front();

  // Send input data to GPU process.
  GpuVideoDecoderInputBufferParam param;
  param.offset_ = 0;
  param.size_ = buffer->GetDataSize();
  param.timestamp_ = buffer->GetTimestamp().InMicroseconds();
  memcpy(input_transfer_buffer_->memory(), buffer->GetData(), param.size_);
  if (!channel_host_ || !channel_host_->Send(
      new GpuVideoDecoderMsg_EmptyThisBuffer(route_id(), param))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_EmptyThisBuffer failed";
  }
}
