// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_channel.h"
#include "chrome/gpu/gpu_video_decoder.h"

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

GpuVideoDecoder::GpuVideoDecoder(
    const GpuVideoDecoderInfoParam* param,
    GpuChannel* channel,
    base::ProcessHandle handle)
    : decoder_host_route_id_(param->decoder_host_route_id_),
      channel_(channel), renderer_handle_(handle) {
}

void GpuVideoDecoder::OnInitialize(const GpuVideoDecoderInitParam& param) {
  init_param_ = param;
  done_param_.success_ = DoInitialize(init_param_, &done_param_);
}

void GpuVideoDecoder::OnUninitialize() {
  DoUninitialize();
}

void GpuVideoDecoder::OnFlush() {
  DoFlush();
}

void GpuVideoDecoder::OnEmptyThisBuffer(
    const GpuVideoDecoderInputBufferParam& buffer) {
  DoEmptyThisBuffer(buffer);
}
void GpuVideoDecoder::OnFillThisBuffer(
    const GpuVideoDecoderOutputBufferParam& frame) {
  DoFillThisBuffer(frame);
}

void GpuVideoDecoder::OnFillThisBufferDoneACK() {
  DoFillThisBufferDoneACK();
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

