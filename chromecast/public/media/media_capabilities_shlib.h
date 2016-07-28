// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_MEDIA_CAPABILITIES_SHLIB_H_
#define CHROMECAST_PUBLIC_MEDIA_MEDIA_CAPABILITIES_SHLIB_H_

#include "chromecast_export.h"
#include "decoder_config.h"

namespace chromecast {
namespace media {

// Interface for specifying platform media capabilities. It allows for more
// detailed information to be provided by the platform compared to the previous
// MediaCodecSupportShlib interface.
class CHROMECAST_EXPORT MediaCapabilitiesShlib {
 public:
  // Return true if the current platform supports the given combination of video
  // codec, profile and level. For a list of supported codecs and profiles see
  // decoder_config.h. The level value is codec specific. For H.264 and HEVC the
  // level value corresponds to level_idc defined in the H.264 / HEVC standard.
  static bool IsSupportedVideoConfig(VideoCodec codec,
                                     VideoProfile profile,
                                     int level) __attribute__((__weak__));
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_MEDIA_CAPABILITIES_SHLIB_H_
