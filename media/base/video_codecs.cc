// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_codecs.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace media {

// The names come from src/third_party/ffmpeg/libavcodec/codec_desc.c
std::string GetCodecName(VideoCodec codec) {
  switch (codec) {
    case kUnknownVideoCodec:
      return "unknown";
    case kCodecH264:
      return "h264";
    case kCodecHEVC:
      return "hevc";
    case kCodecVC1:
      return "vc1";
    case kCodecMPEG2:
      return "mpeg2video";
    case kCodecMPEG4:
      return "mpeg4";
    case kCodecTheora:
      return "theora";
    case kCodecVP8:
      return "vp8";
    case kCodecVP9:
      return "vp9";
  }
  NOTREACHED();
  return "";
}

std::string GetProfileName(VideoCodecProfile profile) {
  switch (profile) {
    case VIDEO_CODEC_PROFILE_UNKNOWN:
      return "unknown";
    case H264PROFILE_BASELINE:
      return "h264 baseline";
    case H264PROFILE_MAIN:
      return "h264 main";
    case H264PROFILE_EXTENDED:
      return "h264 extended";
    case H264PROFILE_HIGH:
      return "h264 high";
    case H264PROFILE_HIGH10PROFILE:
      return "h264 high 10";
    case H264PROFILE_HIGH422PROFILE:
      return "h264 high 4:2:2";
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return "h264 high 4:4:4 predictive";
    case H264PROFILE_SCALABLEBASELINE:
      return "h264 scalable baseline";
    case H264PROFILE_SCALABLEHIGH:
      return "h264 scalable high";
    case H264PROFILE_STEREOHIGH:
      return "h264 stereo high";
    case H264PROFILE_MULTIVIEWHIGH:
      return "h264 multiview high";
    case VP8PROFILE_ANY:
      return "vp8";
    case VP9PROFILE_PROFILE0:
      return "vp9 profile0";
    case VP9PROFILE_PROFILE1:
      return "vp9 profile1";
    case VP9PROFILE_PROFILE2:
      return "vp9 profile2";
    case VP9PROFILE_PROFILE3:
      return "vp9 profile3";
  }
  NOTREACHED();
  return "";
}

bool ParseAVCCodecId(const std::string& codec_id,
                     VideoCodecProfile* profile,
                     uint8_t* level_idc) {
  // Make sure we have avc1.xxxxxx or avc3.xxxxxx , where xxxxxx are hex digits
  if (!base::StartsWith(codec_id, "avc1.", base::CompareCase::SENSITIVE) &&
      !base::StartsWith(codec_id, "avc3.", base::CompareCase::SENSITIVE)) {
    return false;
  }
  uint32_t elem = 0;
  if (codec_id.size() != 11 ||
      !base::HexStringToUInt(base::StringPiece(codec_id).substr(5), &elem)) {
    DVLOG(4) << __FUNCTION__ << ": invalid avc codec id (" << codec_id << ")";
    return false;
  }

  uint8_t level_byte = elem & 0xFF;
  uint8_t constraints_byte = (elem >> 8) & 0xFF;
  uint8_t profile_idc = (elem >> 16) & 0xFF;

  // Check that the lower two bits of |constraints_byte| are zero (those are
  // reserved and must be zero according to ISO IEC 14496-10).
  if (constraints_byte & 3) {
    DVLOG(4) << __FUNCTION__ << ": non-zero reserved bits in codec id "
             << codec_id;
    return false;
  }

  VideoCodecProfile out_profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  // profile_idc values for each profile are taken from ISO IEC 14496-10 and
  // https://en.wikipedia.org/wiki/H.264/MPEG-4_AVC#Profiles
  switch (profile_idc) {
    case 66:
      out_profile = H264PROFILE_BASELINE;
      break;
    case 77:
      out_profile = H264PROFILE_MAIN;
      break;
    case 83:
      out_profile = H264PROFILE_SCALABLEBASELINE;
      break;
    case 86:
      out_profile = H264PROFILE_SCALABLEHIGH;
      break;
    case 88:
      out_profile = H264PROFILE_EXTENDED;
      break;
    case 100:
      out_profile = H264PROFILE_HIGH;
      break;
    case 110:
      out_profile = H264PROFILE_HIGH10PROFILE;
      break;
    case 118:
      out_profile = H264PROFILE_MULTIVIEWHIGH;
      break;
    case 122:
      out_profile = H264PROFILE_HIGH422PROFILE;
      break;
    case 128:
      out_profile = H264PROFILE_STEREOHIGH;
      break;
    case 244:
      out_profile = H264PROFILE_HIGH444PREDICTIVEPROFILE;
      break;
    default:
      DVLOG(1) << "Warning: unrecognized AVC/H.264 profile " << profile_idc;
      return false;
  }

  // TODO(servolk): Take into account also constraint set flags 3 through 5.
  uint8_t constraint_set0_flag = (constraints_byte >> 7) & 1;
  uint8_t constraint_set1_flag = (constraints_byte >> 6) & 1;
  uint8_t constraint_set2_flag = (constraints_byte >> 5) & 1;
  if (constraint_set2_flag && out_profile > H264PROFILE_EXTENDED) {
    out_profile = H264PROFILE_EXTENDED;
  }
  if (constraint_set1_flag && out_profile > H264PROFILE_MAIN) {
    out_profile = H264PROFILE_MAIN;
  }
  if (constraint_set0_flag && out_profile > H264PROFILE_BASELINE) {
    out_profile = H264PROFILE_BASELINE;
  }

  if (level_idc)
    *level_idc = level_byte;

  if (profile)
    *profile = out_profile;

  return true;
}

}  // namespace media
