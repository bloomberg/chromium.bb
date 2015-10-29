// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_codec_support.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chromecast/media/base/media_caps.h"
#include "chromecast/public/media_codec_support_shlib.h"
#include "net/base/mime_util.h"

namespace chromecast {
namespace media {
namespace {

bool IsCodecSupported(const std::string& codec) {
  // FLAC and Opus are supported via the default renderer if the CMA backend
  // does not have explicit support.
  if (codec == "opus" || codec == "flac")
    return true;

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

  // TODO(erickung): This is the temporary solution to allow cast working
  // properly when checking default codec type is supported on not. The change
  // should be reverted once the function is able to call chrome media function
  // directly.
  if (codec == "1" /*PCM*/ || codec == "vorbis" || codec == "opus" ||
      codec == "theora" || codec == "vp8" || codec == "vp8.0" ||
      codec == "vp9" || codec == "vp9.0")
    return true;

  if (codec == "mp3" || codec == "mp4a.66" || codec == "mp4a.67" ||
      codec == "mp4a.68" || codec == "mp4a.69" || codec == "mp4a.6B" ||
      codec == "mp4a.40.2" || codec == "mp4a.40.02" || codec == "mp4a.40.29" ||
      codec == "mp4a.40.5" || codec == "mp4a.40.05" || codec == "mp4a.40")
    return true;

  if (base::StartsWith(codec, "avc1", base::CompareCase::SENSITIVE) ||
      base::StartsWith(codec, "avc3", base::CompareCase::SENSITIVE))
    return true;

  return false;
}

}  // namespace

::media::IsCodecSupportedCB GetIsCodecSupportedOnChromecastCB() {
  return base::Bind(&IsCodecSupported);
}

}  // namespace media
}  // namespace chromecast
