// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_capturer.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "media/base/video_frame.h"

namespace content {

RtcVideoCapturer::RtcVideoCapturer(const media::VideoCaptureSessionId id,
                                   bool is_screencast)
    : is_screencast_(is_screencast),
      delegate_(new RtcVideoCaptureDelegate(id)),
      state_(VIDEO_CAPTURE_STATE_STOPPED) {}

RtcVideoCapturer::~RtcVideoCapturer() {
  DCHECK_EQ(state_, VIDEO_CAPTURE_STATE_STOPPED);
  DVLOG(3) << " RtcVideoCapturer::dtor";
}

cricket::CaptureState RtcVideoCapturer::Start(
    const cricket::VideoFormat& capture_format) {
  DVLOG(3) << " RtcVideoCapturer::Start ";
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    DVLOG(1) << "Got a StartCapture when already started!!! ";
    return cricket::CS_FAILED;
  }

  media::VideoCaptureParams request;
  request.allow_resolution_change = is_screencast_;
  request.requested_format = media::VideoCaptureFormat(
      gfx::Size(capture_format.width, capture_format.height),
      capture_format.framerate(),
      media::PIXEL_FORMAT_I420);

  SetCaptureFormat(&capture_format);

  state_ = VIDEO_CAPTURE_STATE_STARTED;
  first_frame_timestamp_ = media::kNoTimestamp();
  delegate_->StartCapture(
      request,
      base::Bind(&RtcVideoCapturer::OnFrameCaptured, base::Unretained(this)),
      base::Bind(&RtcVideoCapturer::OnStateChange, base::Unretained(this)));
  // Update the desired aspect ratio so that later the video frame can be
  // cropped to meet the requirement if the camera returns a different
  // resolution than the |request|.
  UpdateAspectRatio(capture_format.width, capture_format.height);
  return cricket::CS_STARTING;
}

void RtcVideoCapturer::Stop() {
  DVLOG(3) << " RtcVideoCapturer::Stop ";
  if (state_ == VIDEO_CAPTURE_STATE_STOPPED) {
    DVLOG(1) << "Got a StopCapture while not started.";
    return;
  }

  SetCaptureFormat(NULL);
  state_ = VIDEO_CAPTURE_STATE_STOPPED;
  delegate_->StopCapture();
  SignalStateChange(this, cricket::CS_STOPPED);
}

bool RtcVideoCapturer::IsRunning() {
  return state_ == VIDEO_CAPTURE_STATE_STARTED;
}

bool RtcVideoCapturer::GetPreferredFourccs(std::vector<uint32>* fourccs) {
  if (!fourccs)
    return false;
  fourccs->push_back(cricket::FOURCC_I420);
  return true;
}

bool RtcVideoCapturer::IsScreencast() const {
  return is_screencast_;
}

bool RtcVideoCapturer::GetBestCaptureFormat(const cricket::VideoFormat& desired,
                                            cricket::VideoFormat* best_format) {
  if (!best_format) {
    return false;
  }

  // Chrome does not support capability enumeration.
  // Use the desired format as the best format.
  best_format->width = desired.width;
  best_format->height = desired.height;
  best_format->fourcc = cricket::FOURCC_I420;
  best_format->interval = desired.interval;
  return true;
}

void RtcVideoCapturer::OnFrameCaptured(
    const scoped_refptr<media::VideoFrame>& frame) {
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

  TRACE_EVENT_INSTANT2(
      "rtc_video_capturer",
      "OnFrameCaptured",
      TRACE_EVENT_SCOPE_THREAD,
      "elapsed time",
      captured_frame.elapsed_time,
      "timestamp_ms",
      captured_frame.time_stamp / talk_base::kNumNanosecsPerMillisec);

  // This signals to libJingle that a new VideoFrame is available.
  // libJingle have no assumptions on what thread this signal come from.
  SignalFrameCaptured(this, &captured_frame);
}

void RtcVideoCapturer::OnStateChange(
    RtcVideoCaptureDelegate::CaptureState state) {
  cricket::CaptureState converted_state = cricket::CS_FAILED;
  DVLOG(3) << " RtcVideoCapturer::OnStateChange " << state;
  switch (state) {
    case RtcVideoCaptureDelegate::CAPTURE_STOPPED:
      converted_state = cricket::CS_STOPPED;
      break;
    case RtcVideoCaptureDelegate::CAPTURE_RUNNING:
      converted_state = cricket::CS_RUNNING;
      break;
    case RtcVideoCaptureDelegate::CAPTURE_FAILED:
      // TODO(perkj): Update the comments in the the definition of
      // cricket::CS_FAILED. According to the comments, cricket::CS_FAILED
      // means that the capturer failed to start. But here and in libjingle it
      // is also used if an error occur during capturing.
      converted_state = cricket::CS_FAILED;
      break;
    default:
      NOTREACHED();
      break;
  }
  SignalStateChange(this, converted_state);
}

}  // namespace content
