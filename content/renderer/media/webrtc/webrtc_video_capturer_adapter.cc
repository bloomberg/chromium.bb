// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "media/base/video_frame.h"

namespace content {

WebRtcVideoCapturerAdapter::WebRtcVideoCapturerAdapter(bool is_screencast)
    : is_screencast_(is_screencast),
      running_(false) {
}

WebRtcVideoCapturerAdapter::~WebRtcVideoCapturerAdapter() {
  DVLOG(3) << " WebRtcVideoCapturerAdapter::dtor";
}

void WebRtcVideoCapturerAdapter::SetRequestedFormat(
    const media::VideoCaptureFormat& format) {
  DCHECK_EQ(media::PIXEL_FORMAT_I420, format.pixel_format);
  DVLOG(3) << "WebRtcVideoCapturerAdapter::SetRequestedFormat"
           << " w = " << format.frame_size.width()
           << " h = " << format.frame_size.height();
  cricket::VideoFormat supported_format(format.frame_size.width(),
                                        format.frame_size.height(),
                                        cricket::VideoFormat::FpsToInterval(
                                            format.frame_rate),
                                        cricket::FOURCC_I420);
  SetCaptureFormat(&supported_format);

  // Update the desired aspect ratio so that later the video frame can be
  // cropped to meet the requirement if the camera returns a different
  // resolution than the |request|.
  UpdateAspectRatio(format.frame_size.width(), format.frame_size.height());
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
  DCHECK_EQ(media::VideoFrame::I420, frame->format());
  if (first_frame_timestamp_ == media::kNoTimestamp())
    first_frame_timestamp_ = frame->GetTimestamp();

  // Currently, |fourcc| is always I420.
  cricket::CapturedFrame captured_frame;
  captured_frame.width = frame->coded_size().width();
  captured_frame.height = frame->coded_size().height();
  captured_frame.fourcc = cricket::FOURCC_I420;
  // cricket::CapturedFrame time is in nanoseconds.
  captured_frame.elapsed_time =
      (frame->GetTimestamp() - first_frame_timestamp_).InMicroseconds() *
      base::Time::kNanosecondsPerMicrosecond;
  captured_frame.time_stamp = frame->GetTimestamp().InMicroseconds() *
                              base::Time::kNanosecondsPerMicrosecond;
  // TODO(sheu): we assume contiguous layout of image planes.
  captured_frame.data = frame->data(0);
  captured_frame.data_size =
      media::VideoFrame::AllocationSize(frame->format(), frame->coded_size());
  captured_frame.pixel_height = 1;
  captured_frame.pixel_width = 1;

  // This signals to libJingle that a new VideoFrame is available.
  // libJingle have no assumptions on what thread this signal come from.
  SignalFrameCaptured(this, &captured_frame);
}

}  // namespace content
