// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_codec_support.h"

#include "base/bind.h"
#include "chromecast/media/base/media_caps.h"
#include "chromecast/public/media_codec_support_shlib.h"
#include "net/base/mime_util.h"

namespace chromecast {
namespace media {
namespace {

bool IsCodecSupported(const std::string& codec) {
  MediaCodecSupportShlib::CodecSupport platform_support =
      MediaCodecSupportShlib::IsSupported(codec);
  if (platform_support == MediaCodecSupportShlib::kSupported)
    return true;
  else if (platform_support == MediaCodecSupportShlib::kNotSupported)
    return false;

  if (codec == "aac51") {
    return ::media::HdmiSinkSupportsPcmSurroundSound();
  }
  if (codec == "ac-3" || codec == "mp4a.a5") {
    return ::media::HdmiSinkSupportsAC3();
  }
  if (codec == "ec-3" || codec == "mp4a.a6") {
    return ::media::HdmiSinkSupportsEAC3();
  }

  return true;
}

}  // namespace

net::IsCodecSupportedCB GetIsCodecSupportedOnChromecastCB() {
  return base::Bind(&IsCodecSupported);
}

}  // namespace media
}  // namespace chromecast
