// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_video_encoder.h"

#include "media/base/bind_to_current_loop.h"

namespace extensions {

using InitParameters = WiFiDisplayVideoEncoder::InitParameters;
using VideoEncoderCallback = WiFiDisplayVideoEncoder::VideoEncoderCallback;

WiFiDisplayVideoEncoder::InitParameters::InitParameters() = default;
WiFiDisplayVideoEncoder::InitParameters::InitParameters(const InitParameters&) =
    default;
WiFiDisplayVideoEncoder::InitParameters::~InitParameters() = default;

WiFiDisplayVideoEncoder::WiFiDisplayVideoEncoder() = default;
WiFiDisplayVideoEncoder::~WiFiDisplayVideoEncoder() = default;

void WiFiDisplayVideoEncoder::SetCallbacks(
    const EncodedFrameCallback& encoded_callback,
    const base::Closure& error_callback) {
  encoded_callback_ = media::BindToCurrentLoop(encoded_callback);
  error_callback_ = media::BindToCurrentLoop(error_callback);
}

void WiFiDisplayVideoEncoder::Create(
    const InitParameters& params,
    const VideoEncoderCallback& encoder_callback) {
  NOTIMPLEMENTED();
  encoder_callback.Run(nullptr);
}

}  // namespace extensions
