// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/gpu_video_decoder_host.h"

#include "chrome/common/gpu_messages.h"
#include "media/base/pipeline.h"
#include "content/common/message_router.h"
#include "media/video/video_decode_context.h"

GpuVideoDecoderHost::GpuVideoDecoderHost(MessageRouter* router,
                                         IPC::Message::Sender* ipc_sender,
                                         int context_route_id,
                                         int32 decoder_host_id)
    : router_(router),
      ipc_sender_(ipc_sender),
      context_route_id_(context_route_id),
      message_loop_(NULL),
      event_handler_(NULL),
      context_(NULL),
      width_(0),
      height_(0),
      state_(kStateUninitialized),
      decoder_host_id_(decoder_host_id),
      decoder_id_(0),
      input_buffer_busy_(false),
      current_frame_id_(0) {
}

GpuVideoDecoderHost::~GpuVideoDecoderHost() {}

void GpuVideoDecoderHost::OnChannelError() {
  ipc_sender_ = NULL;
}

bool GpuVideoDecoderHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecoderHost, msg)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_CreateVideoDecoderDone,
                        OnCreateVideoDecoderDone)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_InitializeACK,
                        OnInitializeDone)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_DestroyACK,
                        OnUninitializeDone)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_FlushACK,
                        OnFlushDone)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_PrerollDone,
                        OnPrerollDone)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_EmptyThisBufferACK,
                        OnEmptyThisBufferACK)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_EmptyThisBufferDone,
                        OnProduceVideoSample)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_ConsumeVideoFrame,
                        OnConsumeVideoFrame)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_AllocateVideoFrames,
                        OnAllocateVideoFrames)
    IPC_MESSAGE_HANDLER(GpuVideoDecoderHostMsg_ReleaseAllVideoFrames,
                        OnReleaseAllVideoFrames)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void GpuVideoDecoderHost::Initialize(
    MessageLoop* message_loop, VideoDecodeEngine::EventHandler* event_handler,
    media::VideoDecodeContext* context, const media::VideoCodecConfig& config) {
  DCHECK_EQ(kStateUninitialized, state_);
  DCHECK(!message_loop_);
  message_loop_ = message_loop;
  event_handler_ = event_handler;
  context_ = context;
  width_ = config.width();
  height_ = config.height();

  if (MessageLoop::current() != message_loop) {
    message_loop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &GpuVideoDecoderHost::CreateVideoDecoder));
    return;
  }
  CreateVideoDecoder();
}

void GpuVideoDecoderHost::ConsumeVideoSample(scoped_refptr<Buffer> buffer) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this, &GpuVideoDecoderHost::ConsumeVideoSample, buffer));
    return;
  }

  DCHECK_NE(state_, kStateUninitialized);
  DCHECK_NE(state_, kStateFlushing);

  // We never own input buffers, therefore when client in flush state, it
  // never call us with EmptyThisBuffer.
  if (state_ != kStateNormal)
    return;

  input_buffer_queue_.push_back(buffer);
  SendConsumeVideoSample();
}

void GpuVideoDecoderHost::ProduceVideoFrame(scoped_refptr<VideoFrame> frame) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this, &GpuVideoDecoderHost::ProduceVideoFrame, frame));
    return;
  }

  DCHECK_NE(state_, kStateUninitialized);

  // During flush client of this object will call this method to return all
  // video frames. We should only ignore such method calls if we are in error
  // state.
  if (state_ == kStateError)
    return;

  // Check that video frame is valid.
  if (!frame || frame->format() == media::VideoFrame::EMPTY ||
      frame->IsEndOfStream()) {
    return;
  }

  SendProduceVideoFrame(frame);
}

void GpuVideoDecoderHost::Uninitialize() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &GpuVideoDecoderHost::Uninitialize));
    return;
  }

  if (!ipc_sender_->Send(new GpuVideoDecoderMsg_Destroy(decoder_id_))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_Destroy failed";
    event_handler_->OnError();
  }
}

void GpuVideoDecoderHost::Flush() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &GpuVideoDecoderHost::Flush));
    return;
  }

  state_ = kStateFlushing;
  if (!ipc_sender_->Send(new GpuVideoDecoderMsg_Flush(decoder_id_))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_Flush failed";
    event_handler_->OnError();
    return;
  }

  input_buffer_queue_.clear();
  // TODO(jiesun): because GpuVideoDeocder/GpuVideoDecoder are asynchronously.
  // We need a way to make flush logic more clear. but I think ring buffer
  // should make the busy flag obsolete, therefore I will leave it for now.
  input_buffer_busy_ = false;
}

void GpuVideoDecoderHost::Seek() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &GpuVideoDecoderHost::Seek));
    return;
  }

  if (!ipc_sender_->Send(new GpuVideoDecoderMsg_Preroll(decoder_id_))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_Preroll failed";
    event_handler_->OnError();
    return;
  }
}

void GpuVideoDecoderHost::CreateVideoDecoder() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Add the route so we'll receive messages.
  router_->AddRoute(decoder_host_id_, this);

  if (!ipc_sender_->Send(
          new GpuChannelMsg_CreateVideoDecoder(context_route_id_,
                                               decoder_host_id_))) {
    LOG(ERROR) << "GpuChannelMsg_CreateVideoDecoder failed";
    event_handler_->OnError();
    return;
  }
}

void GpuVideoDecoderHost::OnCreateVideoDecoderDone(int32 decoder_id) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  decoder_id_ = decoder_id;

  // TODO(hclam): Initialize |param| with the right values.
  GpuVideoDecoderInitParam param;
  param.width = width_;
  param.height = height_;

  if (!ipc_sender_->Send(
          new GpuVideoDecoderMsg_Initialize(decoder_id, param))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_Initialize failed";
    event_handler_->OnError();
  }
}

void GpuVideoDecoderHost::OnInitializeDone(
    const GpuVideoDecoderInitDoneParam& param) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  bool success = param.success &&
      base::SharedMemory::IsHandleValid(param.input_buffer_handle);

  if (success) {
    input_transfer_buffer_.reset(
        new base::SharedMemory(param.input_buffer_handle, false));
    success = input_transfer_buffer_->Map(param.input_buffer_size);
  }
  state_ = success ? kStateNormal : kStateError;

  // TODO(hclam): There's too many unnecessary copies for width and height!
  // Need to clean it up.
  // TODO(hclam): Need to fill in more information.
  media::VideoCodecInfo info;
  info.success = success;
  info.stream_info.surface_width = width_;
  info.stream_info.surface_height = height_;
  event_handler_->OnInitializeComplete(info);
}

void GpuVideoDecoderHost::OnUninitializeDone() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  input_transfer_buffer_.reset();
  router_->RemoveRoute(decoder_host_id_);
  context_->ReleaseAllVideoFrames();
  event_handler_->OnUninitializeComplete();
}

void GpuVideoDecoderHost::OnFlushDone() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  state_ = kStateNormal;
  event_handler_->OnFlushComplete();
}

void GpuVideoDecoderHost::OnPrerollDone() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  state_ = kStateNormal;
  event_handler_->OnSeekComplete();
}

void GpuVideoDecoderHost::OnEmptyThisBufferACK() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  input_buffer_busy_ = false;
  SendConsumeVideoSample();
}

void GpuVideoDecoderHost::OnProduceVideoSample() {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(kStateNormal, state_);

  event_handler_->ProduceVideoSample(NULL);
}

void GpuVideoDecoderHost::OnConsumeVideoFrame(int32 frame_id, int64 timestamp,
                                              int64 duration, int32 flags) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  scoped_refptr<VideoFrame> frame;
  if (flags & kGpuVideoEndOfStream) {
    VideoFrame::CreateEmptyFrame(&frame);
  } else {
    frame = video_frame_map_[frame_id];
    DCHECK(frame) << "Invalid frame ID received";

    frame->SetDuration(base::TimeDelta::FromMicroseconds(duration));
    frame->SetTimestamp(base::TimeDelta::FromMicroseconds(timestamp));
  }

  media::PipelineStatistics statistics;
  // TODO(sjl): Fill in statistics.

  event_handler_->ConsumeVideoFrame(frame, statistics);
}

void GpuVideoDecoderHost::OnAllocateVideoFrames(
    int32 n, uint32 width, uint32 height, int32 format) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  DCHECK_EQ(0u, video_frames_.size());

  context_->AllocateVideoFrames(
      n, width, height, static_cast<media::VideoFrame::Format>(format),
      &video_frames_,
      NewRunnableMethod(this,
                        &GpuVideoDecoderHost::OnAllocateVideoFramesDone));
}

void GpuVideoDecoderHost::OnReleaseAllVideoFrames() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  context_->ReleaseAllVideoFrames();
  video_frame_map_.clear();
  video_frames_.clear();
}

void GpuVideoDecoderHost::OnAllocateVideoFramesDone() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this, &GpuVideoDecoderHost::OnAllocateVideoFramesDone));
    return;
  }

  // After video frame allocation is done we add these frames to a map and
  // send them to the GPU process.
  DCHECK(video_frames_.size()) << "No video frames allocated";
  for (size_t i = 0; i < video_frames_.size(); ++i) {
    DCHECK(video_frames_[i]);
    video_frame_map_.insert(
        std::make_pair(current_frame_id_, video_frames_[i]));
    SendVideoFrameAllocated(current_frame_id_, video_frames_[i]);
    ++current_frame_id_;
  }
}

void GpuVideoDecoderHost::SendVideoFrameAllocated(
    int32 frame_id, scoped_refptr<media::VideoFrame> frame) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  std::vector<uint32> textures;
  for (size_t i = 0; i < frame->planes(); ++i) {
    textures.push_back(frame->gl_texture(i));
  }

  if (!ipc_sender_->Send(new GpuVideoDecoderMsg_VideoFrameAllocated(
          decoder_id_, frame_id, textures))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_EmptyThisBuffer failed";
  }
}

void GpuVideoDecoderHost::SendConsumeVideoSample() {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  if (input_buffer_busy_ || input_buffer_queue_.empty())
    return;
  input_buffer_busy_ = true;

  scoped_refptr<Buffer> buffer = input_buffer_queue_.front();
  input_buffer_queue_.pop_front();

  // Send input data to GPU process.
  GpuVideoDecoderInputBufferParam param;
  param.offset = 0;
  param.size = buffer->GetDataSize();
  param.timestamp = buffer->GetTimestamp().InMicroseconds();
  memcpy(input_transfer_buffer_->memory(), buffer->GetData(), param.size);

  if (!ipc_sender_->Send(
          new GpuVideoDecoderMsg_EmptyThisBuffer(decoder_id_, param))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_EmptyThisBuffer failed";
  }
}

void GpuVideoDecoderHost::SendProduceVideoFrame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // TODO(hclam): I should mark a frame being used to DCHECK and make sure
  // user doesn't use it the second time.
  // TODO(hclam): Derive a faster way to lookup the frame ID.
  bool found = false;
  int32 frame_id = 0;
  for (VideoFrameMap::iterator i = video_frame_map_.begin();
       i != video_frame_map_.end(); ++i) {
    if (frame == i->second) {
      frame_id = i->first;
      found = true;
      break;
    }
  }

  DCHECK(found) << "Invalid video frame received";
  if (found && !ipc_sender_->Send(
          new GpuVideoDecoderMsg_ProduceVideoFrame(decoder_id_, frame_id))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_ProduceVideoFrame failed";
  }
}

DISABLE_RUNNABLE_METHOD_REFCOUNT(GpuVideoDecoderHost);
