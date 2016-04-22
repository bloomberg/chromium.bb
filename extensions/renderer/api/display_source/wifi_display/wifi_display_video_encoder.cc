// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_video_encoder.h"

#include "base/logging.h"

namespace extensions {

WiFiDisplayVideoEncoder::InitParameters::InitParameters() = default;
WiFiDisplayVideoEncoder::InitParameters::InitParameters(const InitParameters&) =
    default;
WiFiDisplayVideoEncoder::InitParameters::~InitParameters() = default;

WiFiDisplayVideoEncoder::WiFiDisplayVideoEncoder(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
    : media_task_runner_(std::move(media_task_runner)), send_idr_(false) {
  DCHECK(media_task_runner_);
}

WiFiDisplayVideoEncoder::~WiFiDisplayVideoEncoder() = default;

// static
void WiFiDisplayVideoEncoder::Create(
    const InitParameters& params,
    const VideoEncoderCallback& encoder_callback) {
  NOTIMPLEMENTED();
  encoder_callback.Run(nullptr);
}

void WiFiDisplayVideoEncoder::InsertRawVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    base::TimeTicks reference_time) {
  DCHECK(!encoded_callback_.is_null());
  media_task_runner_->PostTask(
      FROM_HERE, base::Bind(&WiFiDisplayVideoEncoder::InsertFrameOnMediaThread,
                            this, video_frame, reference_time, send_idr_));
  send_idr_ = false;
}

void WiFiDisplayVideoEncoder::RequestIDRPicture() {
  send_idr_ = true;
}

}  // namespace extensions
