// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_video_encoder.h"

namespace extensions {

WiFiDisplayVideoEncoder::InitParameters::InitParameters() = default;
WiFiDisplayVideoEncoder::InitParameters::InitParameters(const InitParameters&) =
    default;
WiFiDisplayVideoEncoder::InitParameters::~InitParameters() = default;

WiFiDisplayVideoEncoder::WiFiDisplayVideoEncoder() = default;
WiFiDisplayVideoEncoder::~WiFiDisplayVideoEncoder() = default;

// static
void WiFiDisplayVideoEncoder::Create(
    const InitParameters& params,
    const VideoEncoderCallback& encoder_callback) {
  NOTIMPLEMENTED();
  encoder_callback.Run(nullptr);
}

}  // namespace extensions
