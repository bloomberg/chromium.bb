// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/media_channel.h"

#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/media/gpu_video_decode_accelerator.h"
#include "content/common/gpu/media/gpu_video_encode_accelerator.h"
#include "content/common/gpu/media/media_messages.h"

namespace content {

namespace {

void SendCreateJpegDecoderResult(
    scoped_ptr<IPC::Message> reply_message,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WeakPtr<GpuChannel> channel,
    scoped_refptr<GpuChannelMessageFilter> filter,
    bool result) {
  GpuChannelMsg_CreateJpegDecoder::WriteReplyParams(reply_message.get(),
                                                    result);
  if (io_task_runner->BelongsToCurrentThread()) {
    filter->Send(reply_message.release());
  } else if (channel) {
    channel->Send(reply_message.release());
  }
}

}  // namespace

MediaChannel::MediaChannel(GpuChannel* channel) : channel_(channel) {}

MediaChannel::~MediaChannel() {}

bool MediaChannel::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

bool MediaChannel::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaChannel, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_CreateVideoDecoder,
                                    OnCreateVideoDecoder)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_CreateVideoEncoder,
                                    OnCreateVideoEncoder)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuChannelMsg_CreateJpegDecoder,
                                    OnCreateJpegDecoder)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaChannel::OnCreateJpegDecoder(int32_t route_id,
                                       IPC::Message* reply_msg) {
  scoped_ptr<IPC::Message> msg(reply_msg);
  if (!jpeg_decoder_) {
    jpeg_decoder_.reset(
        new GpuJpegDecodeAccelerator(channel_, channel_->io_task_runner()));
  }
  jpeg_decoder_->AddClient(
      route_id, base::Bind(&SendCreateJpegDecoderResult, base::Passed(&msg),
                           channel_->io_task_runner(), channel_->AsWeakPtr(),
                           make_scoped_refptr(channel_->filter())));
}

void MediaChannel::OnCreateVideoDecoder(
    int32_t command_buffer_route_id,
    const media::VideoDecodeAccelerator::Config& config,
    int32_t decoder_route_id,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "MediaChannel::OnCreateVideoDecoder");
  GpuCommandBufferStub* stub =
      channel_->LookupCommandBuffer(command_buffer_route_id);
  if (!stub)
    return;
  GpuVideoDecodeAccelerator* decoder = new GpuVideoDecodeAccelerator(
      decoder_route_id, stub, stub->channel()->io_task_runner());
  bool succeeded = decoder->Initialize(config);
  GpuCommandBufferMsg_CreateVideoDecoder::WriteReplyParams(reply_message,
                                                           succeeded);
  Send(reply_message);

  // decoder is registered as a DestructionObserver of this stub and will
  // self-delete during destruction of this stub.
}

void MediaChannel::OnCreateVideoEncoder(const CreateVideoEncoderParams& params,
                                        IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "MediaChannel::OnCreateVideoEncoder");
  GpuCommandBufferStub* stub =
      channel_->LookupCommandBuffer(params.command_buffer_route_id);
  if (!stub)
    return;
  GpuVideoEncodeAccelerator* encoder =
      new GpuVideoEncodeAccelerator(params.encoder_route_id, stub);
  bool succeeded =
      encoder->Initialize(params.input_format, params.input_visible_size,
                          params.output_profile, params.initial_bitrate);
  GpuCommandBufferMsg_CreateVideoEncoder::WriteReplyParams(reply_message,
                                                           succeeded);
  Send(reply_message);

  // encoder is registered as a DestructionObserver of this stub and will
  // self-delete during destruction of this stub.
}

}  // namespace content
