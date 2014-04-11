// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_media_stream_video_track_host.h"

#include "base/logging.h"
#include "media/base/yuv_convert.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "third_party/libyuv/include/libyuv.h"

using media::VideoFrame;
using ppapi::host::HostMessageContext;
using ppapi::MediaStreamVideoTrackShared;

namespace {

const int32_t kDefaultNumberOfBuffers = 4;
const int32_t kMaxNumberOfBuffers = 8;
// Filter mode for scaling frames.
const libyuv::FilterMode kFilterMode = libyuv::kFilterBox;

PP_VideoFrame_Format ToPpapiFormat(VideoFrame::Format format) {
  switch (format) {
    case VideoFrame::YV12:
      return PP_VIDEOFRAME_FORMAT_YV12;
    case VideoFrame::I420:
      return PP_VIDEOFRAME_FORMAT_I420;
    default:
      DVLOG(1) << "Unsupported pixel format " << format;
      return PP_VIDEOFRAME_FORMAT_UNKNOWN;
  }
}

VideoFrame::Format FromPpapiFormat(PP_VideoFrame_Format format) {
  switch (format) {
    case PP_VIDEOFRAME_FORMAT_YV12:
      return VideoFrame::YV12;
    case PP_VIDEOFRAME_FORMAT_I420:
      return VideoFrame::I420;
    default:
      DVLOG(1) << "Unsupported pixel format " << format;
      return VideoFrame::UNKNOWN;
  }
}

// Compute size base on the size of frame received from MediaStreamVideoSink
// and size specified by plugin.
gfx::Size GetTargetSize(const gfx::Size& source, const gfx::Size& plugin) {
  return gfx::Size(plugin.width() ? plugin.width() : source.width(),
                   plugin.height() ? plugin.height() : source.height());
}

// Compute format base on the format of frame received from MediaStreamVideoSink
// and format specified by plugin.
PP_VideoFrame_Format GetTargetFormat(PP_VideoFrame_Format source,
                                     PP_VideoFrame_Format plugin) {
  return plugin != PP_VIDEOFRAME_FORMAT_UNKNOWN ? plugin : source;
}

void ConvertFromMediaVideoFrame(const scoped_refptr<media::VideoFrame>& src,
                                PP_VideoFrame_Format dst_format,
                                const gfx::Size& dst_size,
                                uint8_t* dst) {
  CHECK(src->format() == VideoFrame::YV12 || src->format() == VideoFrame::I420);
  if (dst_format == PP_VIDEOFRAME_FORMAT_BGRA) {
    if (src->coded_size() == dst_size) {
      libyuv::I420ToARGB(src->data(VideoFrame::kYPlane),
                         src->stride(VideoFrame::kYPlane),
                         src->data(VideoFrame::kUPlane),
                         src->stride(VideoFrame::kUPlane),
                         src->data(VideoFrame::kVPlane),
                         src->stride(VideoFrame::kVPlane),
                         dst,
                         dst_size.width() * 4,
                         dst_size.width(),
                         dst_size.height());
    } else {
      media::ScaleYUVToRGB32(src->data(VideoFrame::kYPlane),
                             src->data(VideoFrame::kUPlane),
                             src->data(VideoFrame::kVPlane),
                             dst,
                             src->coded_size().width(),
                             src->coded_size().height(),
                             dst_size.width(),
                             dst_size.height(),
                             src->stride(VideoFrame::kYPlane),
                             src->stride(VideoFrame::kUPlane),
                             dst_size.width() * 4,
                             media::YV12,
                             media::ROTATE_0,
                             media::FILTER_BILINEAR);
    }
  } else if (dst_format == PP_VIDEOFRAME_FORMAT_YV12 ||
             dst_format == PP_VIDEOFRAME_FORMAT_I420) {
    static const size_t kPlanesOrder[][3] = {
        {VideoFrame::kYPlane, VideoFrame::kVPlane,
         VideoFrame::kUPlane},  // YV12
        {VideoFrame::kYPlane, VideoFrame::kUPlane,
         VideoFrame::kVPlane},  // I420
    };
    const int plane_order = (dst_format == PP_VIDEOFRAME_FORMAT_YV12) ? 0 : 1;
    int dst_width = dst_size.width();
    int dst_height = dst_size.height();
    libyuv::ScalePlane(src->data(kPlanesOrder[plane_order][0]),
                       src->stride(kPlanesOrder[plane_order][0]),
                       src->coded_size().width(),
                       src->coded_size().height(),
                       dst,
                       dst_width,
                       dst_width,
                       dst_height,
                       kFilterMode);
    dst += dst_width * dst_height;
    const int src_halfwidth = (src->coded_size().width() + 1) >> 1;
    const int src_halfheight = (src->coded_size().height() + 1) >> 1;
    const int dst_halfwidth = (dst_width + 1) >> 1;
    const int dst_halfheight = (dst_height + 1) >> 1;
    libyuv::ScalePlane(src->data(kPlanesOrder[plane_order][1]),
                       src->stride(kPlanesOrder[plane_order][1]),
                       src_halfwidth,
                       src_halfheight,
                       dst,
                       dst_halfwidth,
                       dst_halfwidth,
                       dst_halfheight,
                       kFilterMode);
    dst += dst_halfwidth * dst_halfheight;
    libyuv::ScalePlane(src->data(kPlanesOrder[plane_order][2]),
                       src->stride(kPlanesOrder[plane_order][2]),
                       src_halfwidth,
                       src_halfheight,
                       dst,
                       dst_halfwidth,
                       dst_halfwidth,
                       dst_halfheight,
                       kFilterMode);
  } else {
    NOTREACHED();
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
      number_of_buffers_(kDefaultNumberOfBuffers),
      source_frame_format_(PP_VIDEOFRAME_FORMAT_UNKNOWN),
      plugin_frame_format_(PP_VIDEOFRAME_FORMAT_UNKNOWN),
      frame_data_size_(0) {
  DCHECK(!track_.isNull());
}

PepperMediaStreamVideoTrackHost::~PepperMediaStreamVideoTrackHost() {
  OnClose();
}

void PepperMediaStreamVideoTrackHost::InitBuffers() {
  gfx::Size size = GetTargetSize(source_frame_size_, plugin_frame_size_);
  DCHECK(!size.IsEmpty());

  PP_VideoFrame_Format format =
      GetTargetFormat(source_frame_format_, plugin_frame_format_);
  DCHECK_NE(format, PP_VIDEOFRAME_FORMAT_UNKNOWN);

  if (format == PP_VIDEOFRAME_FORMAT_BGRA) {
    frame_data_size_ = size.width() * size.height() * 4;
  } else {
    frame_data_size_ =
        VideoFrame::AllocationSize(FromPpapiFormat(format), size);
  }

  DCHECK_GT(frame_data_size_, 0U);
  int32_t buffer_size =
      sizeof(ppapi::MediaStreamBuffer::Video) + frame_data_size_;
  bool result = PepperMediaStreamTrackHostBase::InitBuffers(number_of_buffers_,
                                                            buffer_size);
  CHECK(result);
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

  if (source_frame_size_.IsEmpty()) {
    source_frame_size_ = frame->coded_size();
    source_frame_format_ = ppformat;
    InitBuffers();
  }

  int32_t index = buffer_manager()->DequeueBuffer();
  // Drop frames if the underlying buffer is full.
  if (index < 0) {
    DVLOG(1) << "A frame is dropped.";
    return;
  }

  CHECK(frame->coded_size() == source_frame_size_) << "Frame size is changed";
  CHECK_EQ(ppformat, source_frame_format_) << "Frame format is changed.";

  gfx::Size size = GetTargetSize(source_frame_size_, plugin_frame_size_);
  PP_VideoFrame_Format format =
      GetTargetFormat(source_frame_format_, plugin_frame_format_);
  ppapi::MediaStreamBuffer::Video* buffer =
      &(buffer_manager()->GetBufferPointer(index)->video);
  buffer->header.size = buffer_manager()->buffer_size();
  buffer->header.type = ppapi::MediaStreamBuffer::TYPE_VIDEO;
  buffer->timestamp = frame->timestamp().InSecondsF();
  buffer->format = format;
  buffer->size.width = size.width();
  buffer->size.height = size.height();
  buffer->data_size = frame_data_size_;
  ConvertFromMediaVideoFrame(frame, format, size, buffer->data);
  SendEnqueueBufferMessageToPlugin(index);
}

void PepperMediaStreamVideoTrackHost::DidConnectPendingHostToResource() {
  if (!connected_) {
    MediaStreamVideoSink::AddToVideoTrack(this, track_);
    connected_ = true;
  }
}

int32_t PepperMediaStreamVideoTrackHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperMediaStreamVideoTrackHost, msg)
  PPAPI_DISPATCH_HOST_RESOURCE_CALL(
      PpapiHostMsg_MediaStreamVideoTrack_Configure, OnHostMsgConfigure)
  IPC_END_MESSAGE_MAP()
  return PepperMediaStreamTrackHostBase::OnResourceMessageReceived(msg,
                                                                   context);
}

int32_t PepperMediaStreamVideoTrackHost::OnHostMsgConfigure(
    HostMessageContext* context,
    const MediaStreamVideoTrackShared::Attributes& attributes) {
  CHECK(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

  bool changed = false;
  gfx::Size new_size(attributes.width, attributes.height);
  if (GetTargetSize(source_frame_size_, plugin_frame_size_) !=
      GetTargetSize(source_frame_size_, new_size)) {
    changed = true;
  }
  plugin_frame_size_ = new_size;

  int32_t buffers = attributes.buffers
                        ? std::min(kMaxNumberOfBuffers, attributes.buffers)
                        : kDefaultNumberOfBuffers;
  if (buffers != number_of_buffers_)
    changed = true;
  number_of_buffers_ = buffers;

  if (GetTargetFormat(source_frame_format_, plugin_frame_format_) !=
      GetTargetFormat(source_frame_format_, attributes.format)) {
    changed = true;
  }
  plugin_frame_format_ = attributes.format;

  // If the first frame has been received, we will re-initialize buffers with
  // new settings. Otherwise, we will initialize buffer when we receive
  // the first frame, because plugin can only provide part of attributes
  // which are not enough to initialize buffers.
  if (changed && !source_frame_size_.IsEmpty())
    InitBuffers();

  context->reply_msg = PpapiPluginMsg_MediaStreamVideoTrack_ConfigureReply();
  return PP_OK;
}

}  // namespace content
