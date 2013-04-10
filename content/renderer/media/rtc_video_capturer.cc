// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_capturer.h"

#include "base/bind.h"

namespace content {

RtcVideoCapturer::RtcVideoCapturer(
    const media::VideoCaptureSessionId id,
    VideoCaptureImplManager* vc_manager,
    bool is_screencast)
    : is_screencast_(is_screencast),
      delegate_(new RtcVideoCaptureDelegate(id, vc_manager)),
      state_(VIDEO_CAPTURE_STATE_STOPPED) {
}

RtcVideoCapturer::~RtcVideoCapturer() {
  DCHECK(VIDEO_CAPTURE_STATE_STOPPED);
  DVLOG(3) << " RtcVideoCapturer::dtor";
}

cricket::CaptureState RtcVideoCapturer::Start(
    const cricket::VideoFormat& capture_format) {
  DVLOG(3) << " RtcVideoCapturer::Start ";
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    DVLOG(1) << "Got a StartCapture when already started!!! ";
    return cricket::CS_FAILED;
  }

  media::VideoCaptureCapability cap;
  cap.width = capture_format.width;
  cap.height = capture_format.height;
  cap.frame_rate = capture_format.framerate();
  cap.color = media::VideoCaptureCapability::kI420;

  state_ = VIDEO_CAPTURE_STATE_STARTED;
  start_time_ = base::Time::Now();
  delegate_->StartCapture(cap,
      base::Bind(&RtcVideoCapturer::OnFrameCaptured, base::Unretained(this)),
      base::Bind(&RtcVideoCapturer::OnStateChange, base::Unretained(this)));
  // Update the desired aspect ratio so that later the video frame can be
  // cropped to meet the requirement if the camera returns a different
  // resolution than the |cap|.
  UpdateAspectRatio(cap.width, cap.height);
  return cricket::CS_STARTING;
}

void RtcVideoCapturer::Stop() {
  DVLOG(3) << " RtcVideoCapturer::Stop ";
  if (state_ == VIDEO_CAPTURE_STATE_STOPPED) {
    DVLOG(1) << "Got a StopCapture while not started.";
    return;
  }
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
    const media::VideoCapture::VideoFrameBuffer& buf) {
  // Currently, |fourcc| is always I420.
  cricket::CapturedFrame frame;
  frame.width = buf.width;
  frame.height = buf.height;
  frame.fourcc = cricket::FOURCC_I420;
  frame.data_size = buf.buffer_size;
  // cricket::CapturedFrame time is in nanoseconds.
  frame.elapsed_time = (buf.timestamp - start_time_).InMicroseconds() *
      base::Time::kNanosecondsPerMicrosecond;
  frame.time_stamp = frame.elapsed_time;
  frame.data = buf.memory_pointer;
  frame.pixel_height = 1;
  frame.pixel_width = 1;

  // This signals to libJingle that a new VideoFrame is available.
  // libJingle have no assumptions on what thread this signal come from.
  SignalFrameCaptured(this, &frame);
}

void RtcVideoCapturer::OnStateChange(
    RtcVideoCaptureDelegate::CaptureState state) {
  cricket::CaptureState converted_state = cricket::CS_FAILED;
  switch (state) {
    case RtcVideoCaptureDelegate::CAPTURE_STOPPED:
      converted_state = cricket::CS_STOPPED;
      break;
    case RtcVideoCaptureDelegate::CAPTURE_RUNNING:
      converted_state = cricket::CS_RUNNING;
      break;
    case RtcVideoCaptureDelegate::CAPTURE_FAILED:
      converted_state = cricket::CS_FAILED;
      break;
    default:
      NOTREACHED();
      break;
  }
  SignalStateChange(this, converted_state);
}

}  // namespace content
