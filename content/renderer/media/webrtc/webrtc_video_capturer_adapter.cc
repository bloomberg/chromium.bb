// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/memory/aligned_memory.h"
#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv/convert.h"

namespace content {

WebRtcVideoCapturerAdapter::WebRtcVideoCapturerAdapter(bool is_screencast)
    : is_screencast_(is_screencast),
      running_(false),
      buffer_(NULL),
      buffer_size_(0) {
}

WebRtcVideoCapturerAdapter::~WebRtcVideoCapturerAdapter() {
  DVLOG(3) << " WebRtcVideoCapturerAdapter::dtor";
  base::AlignedFree(buffer_);
}

cricket::CaptureState WebRtcVideoCapturerAdapter::Start(
    const cricket::VideoFormat& capture_format) {
  DCHECK(!running_);
  DVLOG(3) << " WebRtcVideoCapturerAdapter::Start w = " << capture_format.width
           << " h = " << capture_format.height;

  running_ = true;
  return cricket::CS_RUNNING;
}

void WebRtcVideoCapturerAdapter::Stop() {
  DVLOG(3) << " WebRtcVideoCapturerAdapter::Stop ";
  DCHECK(running_);
  running_ = false;
  SetCaptureFormat(NULL);
  SignalStateChange(this, cricket::CS_STOPPED);
}

bool WebRtcVideoCapturerAdapter::IsRunning() {
  return running_;
}

bool WebRtcVideoCapturerAdapter::GetPreferredFourccs(
    std::vector<uint32>* fourccs) {
  if (!fourccs)
    return false;
  fourccs->push_back(cricket::FOURCC_I420);
  return true;
}

bool WebRtcVideoCapturerAdapter::IsScreencast() const {
  return is_screencast_;
}

bool WebRtcVideoCapturerAdapter::GetBestCaptureFormat(
    const cricket::VideoFormat& desired,
    cricket::VideoFormat* best_format) {
  DVLOG(3) << " GetBestCaptureFormat:: "
           << " w = " << desired.width
           << " h = " << desired.height;

  // Capability enumeration is done in MediaStreamVideoSource. The adapter can
  // just use what is provided.
  // Use the desired format as the best format.
  best_format->width = desired.width;
  best_format->height = desired.height;
  best_format->fourcc = cricket::FOURCC_I420;
  best_format->interval = desired.interval;
  return true;
}

void WebRtcVideoCapturerAdapter::OnFrameCaptured(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(media::VideoFrame::I420 == frame->format() ||
         media::VideoFrame::YV12 == frame->format());
  if (first_frame_timestamp_ == media::kNoTimestamp())
    first_frame_timestamp_ = frame->GetTimestamp();

  cricket::CapturedFrame captured_frame;
  captured_frame.width = frame->visible_rect().width();
  captured_frame.height = frame->visible_rect().height();
  // cricket::CapturedFrame time is in nanoseconds.
  captured_frame.elapsed_time =
      (frame->GetTimestamp() - first_frame_timestamp_).InMicroseconds() *
      base::Time::kNanosecondsPerMicrosecond;
  captured_frame.time_stamp = frame->GetTimestamp().InMicroseconds() *
                              base::Time::kNanosecondsPerMicrosecond;
  captured_frame.pixel_height = 1;
  captured_frame.pixel_width = 1;

  // TODO(perkj):
  // Libjingle expects contiguous layout of image planes as input.
  // The only format where that is true in Chrome is I420 where the
  // coded_size == visible_rect().size().
  if (frame->format() != media::VideoFrame::I420 ||
      frame->coded_size() != frame->visible_rect().size()) {
    // Cropping and or switching UV planes is needed.
    UpdateI420Buffer(frame);
    captured_frame.data = buffer_;
    captured_frame.data_size = buffer_size_;
    captured_frame.fourcc = cricket::FOURCC_I420;
  } else {
    captured_frame.fourcc = media::VideoFrame::I420 == frame->format() ?
        cricket::FOURCC_I420 : cricket::FOURCC_YV12;
    captured_frame.data = frame->data(0);
    captured_frame.data_size =
        media::VideoFrame::AllocationSize(frame->format(), frame->coded_size());
  }

  // This signals to libJingle that a new VideoFrame is available.
  // libJingle have no assumptions on what thread this signal come from.
  SignalFrameCaptured(this, &captured_frame);
}

void WebRtcVideoCapturerAdapter::UpdateI420Buffer(
    const scoped_refptr<media::VideoFrame>& src) {
  const int src_width = src->coded_size().width();
  const int src_height = src->coded_size().height();
  const int dst_width = src->visible_rect().width();
  const int dst_height = src->visible_rect().height();
  DCHECK(src_width >= dst_width && src_height >= dst_height);

  const int horiz_crop = src->visible_rect().x();
  const int vert_crop = src->visible_rect().y();

  const uint8* src_y = src->data(media::VideoFrame::kYPlane) +
      (src_width * vert_crop + horiz_crop);
  const int center = (src_width + 1) / 2;
  const uint8* src_u = src->data(media::VideoFrame::kUPlane) +
      (center * vert_crop + horiz_crop) / 2;
  const uint8* src_v = src->data(media::VideoFrame::kVPlane) +
      (center * vert_crop + horiz_crop) / 2;

  const size_t dst_size =
      media::VideoFrame::AllocationSize(src->format(),
                                        src->visible_rect().size());

  if (dst_size != buffer_size_) {
    base::AlignedFree(buffer_);
    buffer_ = reinterpret_cast<uint8*>(
        base::AlignedAlloc(dst_size + media::VideoFrame::kFrameSizePadding,
                           media::VideoFrame::kFrameAddressAlignment));
    buffer_size_ = dst_size;
  }

  uint8* dst_y = buffer_;
  const int dst_stride_y = dst_width;
  uint8* dst_u = dst_y + dst_width * dst_height;
  const int dst_halfwidth = (dst_width + 1) / 2;
  const int dst_halfheight = (dst_height + 1) / 2;
  uint8* dst_v = dst_u + dst_halfwidth * dst_halfheight;

  libyuv::I420Copy(src_y,
                   src->stride(media::VideoFrame::kYPlane),
                   src_u,
                   src->stride(media::VideoFrame::kUPlane),
                   src_v,
                   src->stride(media::VideoFrame::kVPlane),
                   dst_y,
                   dst_stride_y,
                   dst_u,
                   dst_halfwidth,
                   dst_v,
                   dst_halfwidth,
                   dst_width,
                   dst_height);
}

}  // namespace content
