// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_video_decoder.h"

#include "chrome/common/child_thread.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_channel.h"
#include "chrome/gpu/media/fake_gl_video_decode_engine.h"
#include "chrome/gpu/media/fake_gl_video_device.h"
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

void GpuVideoDecoder::OnInitializeComplete(const VideoCodecInfo& info) {
  info_ = info;
  GpuVideoDecoderInitDoneParam param;
  param.success = false;
  param.input_buffer_handle = base::SharedMemory::NULLHandle();

  if (!info.success) {
    SendInitializeDone(param);
    return;
  }

  // TODO(jiesun): Check the assumption of input size < original size.
  param.input_buffer_size = config_.width * config_.height * 3 / 2;
  if (!CreateInputTransferBuffer(param.input_buffer_size,
                                 &param.input_buffer_handle)) {
    SendInitializeDone(param);
    return;
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
  SendFillBufferDone(output_param);
}

void* GpuVideoDecoder::GetDevice() {
  bool ret = gles2_decoder_->MakeCurrent();
  DCHECK(ret) << "Failed to switch context";

  // Simply delegate the method call to GpuVideoDevice.
  return video_device_->GetDevice();
}

void GpuVideoDecoder::AllocateVideoFrames(
    int n, size_t width, size_t height, media::VideoFrame::Format format,
    std::vector<scoped_refptr<media::VideoFrame> >* frames, Task* task) {
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
  DCHECK(video_frame_map_.empty());

  // Save the parameters for allocation.
  pending_allocation_.reset(new PendingAllocation());
  pending_allocation_->n = n;
  pending_allocation_->width = width;
  pending_allocation_->height = height;
  pending_allocation_->format = format;
  pending_allocation_->frames = frames;
  pending_allocation_->task = task;
  SendAllocateVideoFrames(n, width, height, format);
}

void GpuVideoDecoder::ReleaseAllVideoFrames() {
  // This method will first call to GpuVideoDevice to release all the resource
  // associated with a VideoFrame.
  //
  // And then we'll call GpuVideoDevice::ReleaseVideoFrame() to remove the set
  // of Gl textures associated with the context.
  //
  // And finally we'll send IPC commands to IpcVideoDecoder to destroy all
  // GL textures generated.
  bool ret = gles2_decoder_->MakeCurrent();
  DCHECK(ret) << "Failed to switch context";

  for (VideoFrameMap::iterator i = video_frame_map_.begin();
       i != video_frame_map_.end(); ++i) {
    video_device_->ReleaseVideoFrame(i->second);
  }
  video_frame_map_.clear();
  SendReleaseAllVideoFrames();
}

void GpuVideoDecoder::UploadToVideoFrame(void* buffer,
                                         scoped_refptr<media::VideoFrame> frame,
                                         Task* task) {
  // This method is called by VideoDecodeEngine to upload a buffer to a
  // VideoFrame. We should just delegate this to GpuVideoDevice which contains
  // the actual implementation.
  bool ret = gles2_decoder_->MakeCurrent();
  DCHECK(ret) << "Failed to switch context";

  // Actually doing the upload on the main thread.
  ret = video_device_->UploadToVideoFrame(buffer, frame);
  DCHECK(ret) << "Failed to upload video content to a VideoFrame.";
  task->Run();
  delete task;
}

void GpuVideoDecoder::Destroy(Task* task) {
  //  TODO(hclam): I still need to think what I should do here.
}

GpuVideoDecoder::GpuVideoDecoder(
    const GpuVideoDecoderInfoParam* param,
    GpuChannel* channel,
    base::ProcessHandle handle,
    gpu::gles2::GLES2Decoder* decoder)
    : decoder_host_route_id_(param->decoder_host_route_id),
      pending_output_requests_(0),
      channel_(channel),
      renderer_handle_(handle),
      gles2_decoder_(decoder) {
  memset(&config_, 0, sizeof(config_));
  memset(&info_, 0, sizeof(info_));

  // TODO(jiesun): find a better way to determine which VideoDecodeEngine
  // to return on current platform.
  decode_engine_.reset(new FakeGlVideoDecodeEngine());
  video_device_.reset(new FakeGlVideoDevice());
}

void GpuVideoDecoder::OnInitialize(const GpuVideoDecoderInitParam& param) {
  // TODO(hclam): Initialize the VideoDecodeContext first.
  // TODO(jiesun): codec id should come from |param|.
  config_.codec = media::kCodecH264;
  config_.width = param.width;
  config_.height = param.height;
  config_.opaque_context = NULL;
  decode_engine_->Initialize(NULL, this, this, config_);
}

void GpuVideoDecoder::OnUninitialize() {
  decode_engine_->Uninitialize();
}

void GpuVideoDecoder::OnFlush() {
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
  bool ret = gles2_decoder_->MakeCurrent();
  DCHECK(ret) << "Failed to switch context";

  if (info_.stream_info.surface_type == VideoFrame::TYPE_SYSTEM_MEMORY) {
    pending_output_requests_++;
  } else {
  }
}

void GpuVideoDecoder::OnFillThisBufferDoneACK() {
  if (info_.stream_info.surface_type == VideoFrame::TYPE_SYSTEM_MEMORY) {
    pending_output_requests_--;
    if (pending_output_requests_) {
      decode_engine_->ProduceVideoFrame(frame_);
    }
  }
}

void GpuVideoDecoder::OnVideoFrameAllocated(int32 frame_id,
                                            std::vector<uint32> textures) {
  // This method is called in response to a video frame allocation request sent
  // to the Renderer process.
  // We should use the textures to generate a VideoFrame by using
  // GpuVideoDevice. The VideoFrame created is added to the internal map.
  // If we have generated enough VideoFrame, we call |allocation_callack_| to
  // complete the allocation process.
  for (size_t i = 0; i < textures.size(); ++i) {
    media::VideoFrame::GlTexture gl_texture;
    // Translate the client texture id to service texture id.
    bool ret = gles2_decoder_->GetServiceTextureId(textures[i], &gl_texture);
    DCHECK(ret) << "Cannot translate client texture ID to service ID";
    textures[i] = gl_texture;
  }

  scoped_refptr<media::VideoFrame> frame;
  bool ret = video_device_->CreateVideoFrameFromGlTextures(
      pending_allocation_->width, pending_allocation_->height,
      pending_allocation_->format, textures, &frame);

  DCHECK(ret) << "Failed to allocation VideoFrame from GL textures)";
  pending_allocation_->frames->push_back(frame);
  video_frame_map_.insert(std::make_pair(frame_id, frame));

  if (video_frame_map_.size() == pending_allocation_->n) {
    pending_allocation_->task->Run();
    delete pending_allocation_->task;
    pending_allocation_.reset();
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

void GpuVideoDecoder::SendAllocateVideoFrames(
    int n, size_t width, size_t height, media::VideoFrame::Format format) {
  // TODO(hclam): Actually send the message.
}

void GpuVideoDecoder::SendReleaseAllVideoFrames() {
  if (!channel_->Send(
          new GpuVideoDecoderHostMsg_ReleaseAllVideoFrames(route_id()))) {
    LOG(ERROR) << "GpuVideoDecoderMsg_ReleaseAllVideoFrames failed";
  }
}
