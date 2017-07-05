// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_video_source.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"

namespace content {

MockMediaStreamVideoSource::MockMediaStreamVideoSource()
    : MockMediaStreamVideoSource(false) {}

MockMediaStreamVideoSource::MockMediaStreamVideoSource(
    bool respond_to_request_refresh_frame)
    : respond_to_request_refresh_frame_(respond_to_request_refresh_frame),
      max_requested_height_(0),
      max_requested_width_(0),
      max_requested_frame_rate_(0.0),
      attempted_to_start_(false) {}

MockMediaStreamVideoSource::MockMediaStreamVideoSource(
    const media::VideoCaptureFormat& format,
    bool respond_to_request_refresh_frame)
    : format_(format),
      respond_to_request_refresh_frame_(respond_to_request_refresh_frame),
      max_requested_height_(format.frame_size.height()),
      max_requested_width_(format.frame_size.width()),
      max_requested_frame_rate_(format.frame_rate),
      attempted_to_start_(false) {}

MockMediaStreamVideoSource::~MockMediaStreamVideoSource() {}

void MockMediaStreamVideoSource::StartMockedSource() {
  DCHECK(attempted_to_start_);
  attempted_to_start_ = false;
  OnStartDone(MEDIA_DEVICE_OK);
}

void MockMediaStreamVideoSource::FailToStartMockedSource() {
  DCHECK(attempted_to_start_);
  attempted_to_start_ = false;
  OnStartDone(MEDIA_DEVICE_TRACK_START_FAILURE);
}

void MockMediaStreamVideoSource::RequestRefreshFrame() {
  DCHECK(!frame_callback_.is_null());
  if (respond_to_request_refresh_frame_) {
    const scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateColorFrame(format_.frame_size, 0, 0, 0,
                                            base::TimeDelta());
    io_task_runner()->PostTask(
        FROM_HERE, base::Bind(frame_callback_, frame, base::TimeTicks()));
  }
}

void MockMediaStreamVideoSource::StartSourceImpl(
    const VideoCaptureDeliverFrameCB& frame_callback) {
  DCHECK(frame_callback_.is_null());
  attempted_to_start_ = true;
  frame_callback_ = frame_callback;
}

void MockMediaStreamVideoSource::StopSourceImpl() {
}

base::Optional<media::VideoCaptureFormat>
MockMediaStreamVideoSource::GetCurrentFormat() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Optional<media::VideoCaptureFormat>(format_);
}

void MockMediaStreamVideoSource::DeliverVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK(!frame_callback_.is_null());
  io_task_runner()->PostTask(
      FROM_HERE, base::Bind(frame_callback_, frame, base::TimeTicks()));
}

}  // namespace content
