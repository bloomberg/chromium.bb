// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_MEDIA_CAPS_
#define CHROMECAST_MEDIA_BASE_MEDIA_CAPS_

#include "ui/gfx/geometry/size.h"

namespace chromecast {
namespace media {

class MediaCapabilities {
 public:
  enum HdmiSinkCodec {
    kSinkCodecAc3 = 1,
    kSinkCodecDts = 1 << 1,
    kSinkCodecDtsHd = 1 << 2,
    kSinkCodecEac3 = 1 << 3,
    kSinkCodecPcmSurroundSound = 1 << 4,
  };

  // Records the known supported codecs for the current HDMI sink, as a bit mask
  // of HdmiSinkCodec values.
  static void SetHdmiSinkCodecs(unsigned int codecs_mask);

  static bool HdmiSinkSupportsAC3();
  static bool HdmiSinkSupportsDTS();
  static bool HdmiSinkSupportsDTSHD();
  static bool HdmiSinkSupportsEAC3();
  static bool HdmiSinkSupportsPcmSurroundSound();

  static void ScreenResolutionChanged(const gfx::Size& res);
  static gfx::Size GetScreenResolution();

 private:
  static unsigned int g_hdmi_codecs;
  static gfx::Size g_screen_resolution;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_MEDIA_CAPS_
