// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_decode_accelerator.h"

#include <GLES2/gl2.h>
#include "base/logging.h"

namespace media {

VideoDecodeAccelerator::Config::Config(VideoCodecProfile video_codec_profile)
    : profile(video_codec_profile) {}

VideoDecodeAccelerator::Config::Config(
    const VideoDecoderConfig& video_decoder_config)
    : profile(video_decoder_config.profile()),
      is_encrypted(video_decoder_config.is_encrypted()) {}

std::string VideoDecodeAccelerator::Config::AsHumanReadableString() const {
  std::ostringstream s;
  s << "profile: " << GetProfileName(profile) << " encrypted? "
    << (is_encrypted ? "true" : "false");
  return s.str();
}

void VideoDecodeAccelerator::Client::NotifyCdmAttached(bool success) {
  NOTREACHED() << "By default CDM is not supported.";
}

VideoDecodeAccelerator::~VideoDecodeAccelerator() {}

void VideoDecodeAccelerator::SetCdm(int cdm_id) {
  NOTREACHED() << "By default CDM is not supported.";
}

bool VideoDecodeAccelerator::CanDecodeOnIOThread() {
  // GPU process subclasses must override this.
  LOG(FATAL) << "This should only get called in the GPU process";
  return false;  // not reached
}

GLenum VideoDecodeAccelerator::GetSurfaceInternalFormat() const {
  return GL_RGBA;
}

VideoDecodeAccelerator::SupportedProfile::SupportedProfile()
    : profile(media::VIDEO_CODEC_PROFILE_UNKNOWN) {}

VideoDecodeAccelerator::SupportedProfile::~SupportedProfile() {}

VideoDecodeAccelerator::Capabilities::Capabilities() : flags(NO_FLAGS) {}

VideoDecodeAccelerator::Capabilities::~Capabilities() {}

std::string VideoDecodeAccelerator::Capabilities::AsHumanReadableString()
    const {
  std::ostringstream s;
  s << "[";
  for (const SupportedProfile& sp : supported_profiles) {
    s << " " << GetProfileName(sp.profile) << ": " << sp.min_resolution.width()
      << "x" << sp.min_resolution.height() << "->" << sp.max_resolution.width()
      << "x" << sp.max_resolution.height();
  }
  s << "]";
  return s.str();
}

} // namespace media

namespace std {

void default_delete<media::VideoDecodeAccelerator>::operator()(
    media::VideoDecodeAccelerator* vda) const {
  vda->Destroy();
}

}  // namespace std
