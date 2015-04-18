// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

// TODO(servolk): Temp definitions to fix Chromecast build until media mime
// type refactoring checks are properly upstreamed.
namespace net {

typedef base::Callback<bool(const std::string&)> IsCodecSupportedCB;

static bool IsValidH264BaselineProfile(const std::string& profile_str) {
  uint32 constraint_set_bits;
  if (profile_str.size() != 4 ||
      profile_str[0] != '4' ||
      profile_str[1] != '2' ||
      profile_str[3] != '0' ||
      !base::HexStringToUInt(base::StringPiece(profile_str.c_str() + 2, 1),
                             &constraint_set_bits)) {
    return false;
  }

  return constraint_set_bits >= 8;
}

static bool IsValidH264Level(const std::string& level_str) {
  uint32 level;
  if (level_str.size() != 2 || !base::HexStringToUInt(level_str, &level))
    return false;

  // Valid levels taken from Table A-1 in ISO-14496-10.
  // Essentially |level_str| is toHex(10 * level).
  return ((level >= 10 && level <= 13) ||
          (level >= 20 && level <= 22) ||
          (level >= 30 && level <= 32) ||
          (level >= 40 && level <= 42) ||
          (level >= 50 && level <= 51));
}

static bool ParseH264CodecID(const std::string& codec_id,
                             bool* is_ambiguous) {
  // Make sure we have avc1.xxxxxx or avc3.xxxxxx
  if (codec_id.size() != 11 ||
      (!StartsWithASCII(codec_id, "avc1.", true) &&
       !StartsWithASCII(codec_id, "avc3.", true))) {
    return false;
  }

  std::string profile = StringToUpperASCII(codec_id.substr(5, 4));
  if (IsValidH264BaselineProfile(profile) ||
      profile == "4D40" || profile == "6400") {
    *is_ambiguous = !IsValidH264Level(StringToUpperASCII(codec_id.substr(9)));
  } else {
    *is_ambiguous = true;
  }

  return true;
}

bool DefaultIsCodecSupported(const std::string& codec) {
  if (codec == "1" /*PCM*/ || codec == "vorbis" || codec == "opus" ||
      codec == "theora" || codec == "vp8" || codec == "vp8.0" ||
      codec == "vp9" || codec == "vp9.0")
    return true;

  if (codec == "mp3" || codec == "mp4a.66" || codec == "mp4a.67" ||
      codec == "mp4a.68" || codec == "mp4a.69" || codec == "mp4a.6B" ||
      codec == "mp4a.40.2" || codec == "mp4a.40.02" || codec == "mp4a.40.29" ||
      codec == "mp4a.40.5" || codec == "mp4a.40.05" || codec == "mp4a.40")
    return true;

#if defined(ENABLE_MPEG2TS_STREAM_PARSER)
  // iOS 3.0 to 3.1.2 used non-standard codec ids for H.264 Baseline and Main
  // profiles in HTTP Live Streaming (HLS). Apple recommends using these
  // non-standard strings for compatibility with older iOS devices, and so many
  // HLS apps still use these. See
  // https://developer.apple.com/library/ios/documentation/NetworkingInternet/Conceptual/StreamingMediaGuide/FrequentlyAskedQuestions/FrequentlyAskedQuestions.html
  // mp4a.40.34 is also Apple-specific name for MP3 in mpeg2ts container for HLS
  if (codec == "avc1.66.30" || codec == "avc1.77.30" || codec == "mp4a.40.34")
    return true;
#endif


  bool is_ambiguous = true;
  if (codec == "avc1" || codec == "avc3" ||
      ParseH264CodecID(codec, &is_ambiguous)) {
    return true;
  }

  // Unknown codec id
  return false;
}

}

namespace chromecast {
namespace media {

// This is a default implementation of GetIsCodecSupportedOnChromecastCB that is
// going to be used for brandings other than 'Chrome'. Most platforms will want
// to use their own custom implementations of the IsCodecSupportedCB callback.
net::IsCodecSupportedCB GetIsCodecSupportedOnChromecastCB() {
  return base::Bind(&net::DefaultIsCodecSupported);
}

}  // namespace media
}  // namespace chromecast

