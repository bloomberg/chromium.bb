// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_media_stream_video_track_host.h"

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/shared_impl/media_stream_buffer.h"

using media::VideoFrame;

namespace {

// TODO(penghuang): make it configurable.
const int32_t kNumberOfFrames = 4;

PP_VideoFrame_Format ToPpapiFormat(VideoFrame::Format format) {
  switch (format) {
    case VideoFrame::YV12:
      return PP_VIDEOFRAME_FORMAT_YV12;
    case VideoFrame::YV16:
      return PP_VIDEOFRAME_FORMAT_YV16;
    case VideoFrame::I420:
      return PP_VIDEOFRAME_FORMAT_I420;
    case VideoFrame::YV12A:
      return PP_VIDEOFRAME_FORMAT_YV12A;
    case VideoFrame::YV12J:
      return PP_VIDEOFRAME_FORMAT_YV12J;
    default:
      DVLOG(1) << "Unsupported pixel format " << format;
      return PP_VIDEOFRAME_FORMAT_UNKNOWN;
  }
}

}  // namespace

namespace content {

PepperMediaStreamVideoTrackHost::PepperMediaStreamVideoTrackHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const blink::WebMediaStreamTrack& track)
    : PepperMediaStreamTrackHostBase(host, instance, resource),
      track_(track),
      connected_(false),
      frame_format_(VideoFrame::UNKNOWN),
      frame_data_size_(0) {
  DCHECK(!track_.isNull());
}

PepperMediaStreamVideoTrackHost::~PepperMediaStreamVideoTrackHost() {
  OnClose();
}

void PepperMediaStreamVideoTrackHost::OnClose() {
  if (connected_) {
    MediaStreamVideoSink::RemoveFromVideoTrack(this, track_);
    connected_ = false;
  }
}

void PepperMediaStreamVideoTrackHost::OnVideoFrame(
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(frame);
  // TODO(penghuang): Check |frame->end_of_stream()| and close the track.
  PP_VideoFrame_Format ppformat = ToPpapiFormat(frame->format());
  if (ppformat == PP_VIDEOFRAME_FORMAT_UNKNOWN)
    return;

  if (frame_size_ != frame->coded_size() || frame_format_ != frame->format()) {
    frame_size_ = frame->coded_size();
    frame_format_ = frame->format();
    // TODO(penghuang): Support changing |frame_size_| & |frame_format_| more
    // than once.
    DCHECK(!frame_data_size_);
    frame_data_size_ = VideoFrame::AllocationSize(frame_format_, frame_size_);
    int32_t size = sizeof(ppapi::MediaStreamBuffer::Video) + frame_data_size_;
    bool result = InitBuffers(kNumberOfFrames, size);
    // TODO(penghuang): Send PP_ERROR_NOMEMORY to plugin.
    CHECK(result);
  }

  int32_t index = buffer_manager()->DequeueBuffer();
  // Drop frames if the underlying buffer is full.
  if (index < 0)
    return;

  // TODO(penghuang): support format conversion and size scaling.
  ppapi::MediaStreamBuffer::Video* buffer =
      &(buffer_manager()->GetBufferPointer(index)->video);
  buffer->header.size = buffer_manager()->buffer_size();
  buffer->header.type = ppapi::MediaStreamBuffer::TYPE_VIDEO;
  buffer->timestamp = frame->GetTimestamp().InSecondsF();
  buffer->format = ppformat;
  buffer->size.width = frame->coded_size().width();
  buffer->size.height = frame->coded_size().height();
  buffer->data_size = frame_data_size_;

  COMPILE_ASSERT(VideoFrame::kYPlane == 0, y_plane_should_be_0);
  COMPILE_ASSERT(VideoFrame::kUPlane == 1, u_plane_should_be_1);
  COMPILE_ASSERT(VideoFrame::kVPlane == 2, v_plane_should_be_2);

  uint8_t* dst = buffer->data;
  size_t num_planes = VideoFrame::NumPlanes(frame->format());
  for (size_t i = 0; i < num_planes; ++i) {
    const uint8_t* src = frame->data(i);
    const size_t row_bytes = frame->row_bytes(i);
    const size_t src_stride = frame->stride(i);
    int rows = frame->rows(i);
    for (int j = 0; j < rows; ++j) {
      memcpy(dst, src, row_bytes);
      dst += row_bytes;
      src += src_stride;
    }
  }

  SendEnqueueBufferMessageToPlugin(index);
}

void PepperMediaStreamVideoTrackHost::DidConnectPendingHostToResource() {
  if (!connected_) {
    MediaStreamVideoSink::AddToVideoTrack(this, track_);
    connected_ = true;
  }
}

}  // namespace content
