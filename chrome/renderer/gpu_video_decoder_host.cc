// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/gpu_video_decoder_host.h"

#include "chrome/common/gpu_messages.h"
#include "chrome/renderer/gpu_video_service_host.h"
#include "chrome/renderer/render_thread.h"

GpuVideoDecoderHost::GpuVideoDecoderHost(GpuVideoServiceHost* service_host,
                                         IPC::Message::Sender* ipc_sender,
                                         int context_route_id)
    : gpu_video_service_host_(service_host),
      ipc_sender_(ipc_sender),
      context_route_id_(context_route_id),
      event_handler_(NULL),
      buffer_id_serial_(0),
      state_(kStateUninitialized),
      input_buffer_busy_(false) {
  memset(&config_, 0, sizeof(config_));
  memset(&done_param_, 0, sizeof(done_param_));
}

void GpuVideoDecoderHost::OnChannelError() {
  ipc_sender_ = NULL;
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
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void GpuVideoDecoderHost::Initialize(
    MessageLoop* message_loop, VideoDecodeEngine::EventHandler* event_handler,
    media::VideoDecodeContext* context, const media::VideoCodecConfig& config) {
  // TODO(hclam): Call |event_handler| here.
  DCHECK_EQ(state_, kStateUninitialized);

  // Save the event handler before we perform initialization operations so
  // that we can report initialization events.
  event_handler_ = event_handler;

  // TODO(hclam): This create video decoder operation is synchronous, need to
  // make it asynchronous.
  decoder_info_.context_id = context_route_id_;
  if (!ipc_sender_->Send(
          new GpuChannelMsg_CreateVideoDecoder(&decoder_info_))) {
    LOG(ERROR) << "GpuChannelMsg_CreateVideoDecoder failed";
    return;
  }

  // Add the route so we'll receive messages.
  gpu_video_service_host_->AddRoute(my_route_id(), this);

  // Save the configuration parameters.
  config_ = config;

  // TODO(hclam): Initialize |param| with the right values.
  GpuVideoDecoderInitParam param;
  param.width = config.width;
  param.height = config.height;

  if (!ipc_sender_ || !ipc_sender_->Send(
      new GpuVideoDecoderMsg_Initialize(route_id(), param))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_Initialize failed";
    return;
  }
}

void GpuVideoDecoderHost::ConsumeVideoSample(scoped_refptr<Buffer> buffer) {
  DCHECK_NE(state_, kStateUninitialized);
  DCHECK_NE(state_, kStateFlushing);

  // We never own input buffers, therefore when client in flush state, it
  // never call us with EmptyThisBuffer.
  if (state_ != kStateNormal)
    return;

  input_buffer_queue_.push_back(buffer);
  SendInputBufferToGpu();
}

void GpuVideoDecoderHost::ProduceVideoFrame(scoped_refptr<VideoFrame> frame) {
  DCHECK_NE(state_, kStateUninitialized);

  // Depends on who provides buffer. client could return buffer to
  // us while flushing.
  if (state_ == kStateError)
    return;

  // TODO(hclam): We should keep an IDMap to convert between a frame a buffer
  // ID so that we can signal GpuVideoDecoder in GPU process to use the buffer.
  // This eliminates one conversion step.
}

void GpuVideoDecoderHost::Uninitialize() {
  if (!ipc_sender_ || !ipc_sender_->Send(
      new GpuVideoDecoderMsg_Destroy(route_id()))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_Destroy failed";
    return;
  }

  gpu_video_service_host_->RemoveRoute(my_route_id());
  return;
}

void GpuVideoDecoderHost::Flush() {
  state_ = kStateFlushing;
  if (!ipc_sender_ || !ipc_sender_->Send(
      new GpuVideoDecoderMsg_Flush(route_id()))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_Flush failed";
    return;
  }
  input_buffer_queue_.clear();
  // TODO(jiesun): because GpuVideoDeocder/GpuVideoDecoder are asynchronously.
  // We need a way to make flush logic more clear. but I think ring buffer
  // should make the busy flag obsolete, therefore I will leave it for now.
  input_buffer_busy_ = false;
  return;
}

void GpuVideoDecoderHost::Seek() {
  // TODO(hclam): Implement.
}

void GpuVideoDecoderHost::OnInitializeDone(
    const GpuVideoDecoderInitDoneParam& param) {
  done_param_ = param;
  bool success = false;

  do {
    if (!param.success)
      break;

    if (!base::SharedMemory::IsHandleValid(param.input_buffer_handle))
      break;
    input_transfer_buffer_.reset(
        new base::SharedMemory(param.input_buffer_handle, false));
    if (!input_transfer_buffer_->Map(param.input_buffer_size))
      break;

    success = true;
  } while (0);

  state_ = success ? kStateNormal : kStateError;

  media::VideoCodecInfo info;
  info.success = success;
  // TODO(hclam): There's too many unnecessary copies for width and height!
  // Need to clean it up.
  // TODO(hclam): Need to fill in more information.
  info.stream_info.surface_width = config_.width;
  info.stream_info.surface_height = config_.height;
  event_handler_->OnInitializeComplete(info);
}

void GpuVideoDecoderHost::OnUninitializeDone() {
  input_transfer_buffer_.reset();
  event_handler_->OnUninitializeComplete();
}

void GpuVideoDecoderHost::OnFlushDone() {
  state_ = kStateNormal;
  event_handler_->OnFlushComplete();
}

void GpuVideoDecoderHost::OnEmptyThisBufferDone() {
  event_handler_->ProduceVideoSample(NULL);
}

void GpuVideoDecoderHost::OnConsumeVideoFrame(int32 frame_id, int64 timestamp,
                                              int64 duration, int32 flags) {
  scoped_refptr<VideoFrame> frame;

  if (flags & kGpuVideoEndOfStream) {
    VideoFrame::CreateEmptyFrame(&frame);
  } else {
    // TODO(hclam): Use |frame_id| to find the VideoFrame.
  }

  // TODO(hclam): Call the event handler.
  event_handler_->ConsumeVideoFrame(frame);
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
  param.offset = 0;
  param.size = buffer->GetDataSize();
  param.timestamp = buffer->GetTimestamp().InMicroseconds();
  memcpy(input_transfer_buffer_->memory(), buffer->GetData(), param.size);
  if (!ipc_sender_ || !ipc_sender_->Send(
      new GpuVideoDecoderMsg_EmptyThisBuffer(route_id(), param))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_EmptyThisBuffer failed";
  }
}
