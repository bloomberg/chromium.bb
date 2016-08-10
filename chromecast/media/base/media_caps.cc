// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/media_caps.h"

namespace chromecast {
namespace media {

unsigned int MediaCapabilities::g_hdmi_codecs = 0;
gfx::Size MediaCapabilities::g_screen_resolution(0, 0);

void MediaCapabilities::SetHdmiSinkCodecs(unsigned int codecs_mask) {
  g_hdmi_codecs = codecs_mask;
}

bool MediaCapabilities::HdmiSinkSupportsAC3() {
  return g_hdmi_codecs & HdmiSinkCodec::kSinkCodecAc3;
}

bool MediaCapabilities::HdmiSinkSupportsDTS() {
  return g_hdmi_codecs & HdmiSinkCodec::kSinkCodecDts;
}

bool MediaCapabilities::HdmiSinkSupportsDTSHD() {
  return g_hdmi_codecs & HdmiSinkCodec::kSinkCodecDtsHd;
}

bool MediaCapabilities::HdmiSinkSupportsEAC3() {
  return g_hdmi_codecs & HdmiSinkCodec::kSinkCodecEac3;
}

bool MediaCapabilities::HdmiSinkSupportsPcmSurroundSound() {
  return g_hdmi_codecs & HdmiSinkCodec::kSinkCodecPcmSurroundSound;
}

void MediaCapabilities::ScreenResolutionChanged(const gfx::Size& res) {
  VLOG(1) << __FUNCTION__ << " resolution=" << res.ToString();
  g_screen_resolution = res;
}

gfx::Size MediaCapabilities::GetScreenResolution() {
  return g_screen_resolution;
}

}  // namespace media
}  // namespace chromecast
