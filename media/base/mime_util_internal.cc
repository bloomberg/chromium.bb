// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mime_util_internal.h"

#include <map>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "media/base/media_client.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/media_features.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "media/base/android/media_codec_util.h"
#endif

namespace media {
namespace internal {

// Wrapped to avoid static initializer startup cost.
const std::map<std::string, MimeUtil::Codec>& GetStringToCodecMap() {
  static const std::map<std::string, MimeUtil::Codec> kStringToCodecMap = {
    // We only allow this for WAV so it isn't ambiguous.
    {"1", MimeUtil::PCM},
    // avc1/avc3.XXXXXX may be unambiguous; handled by ParseAVCCodecId().
    // hev1/hvc1.XXXXXX may be unambiguous; handled by ParseHEVCCodecID().
    // vp9, vp9.0, vp09.xx.xx.xx.xx.xx.xx.xx may be unambiguous; handled by
    // ParseVp9CodecID().
    {"mp3", MimeUtil::MP3},
    // Following is the list of RFC 6381 compliant audio codec strings:
    //   mp4a.66     - MPEG-2 AAC MAIN
    //   mp4a.67     - MPEG-2 AAC LC
    //   mp4a.68     - MPEG-2 AAC SSR
    //   mp4a.69     - MPEG-2 extension to MPEG-1 (MP3)
    //   mp4a.6B     - MPEG-1 audio (MP3)
    //   mp4a.40.2   - MPEG-4 AAC LC
    //   mp4a.40.02  - MPEG-4 AAC LC (leading 0 in aud-oti for compatibility)
    //   mp4a.40.5   - MPEG-4 HE-AAC v1 (AAC LC + SBR)
    //   mp4a.40.05  - MPEG-4 HE-AAC v1 (AAC LC + SBR) (leading 0 in aud-oti
    //                 for compatibility)
    //   mp4a.40.29  - MPEG-4 HE-AAC v2 (AAC LC + SBR + PS)
    {"mp4a.66", MimeUtil::MPEG2_AAC},
    {"mp4a.67", MimeUtil::MPEG2_AAC},
    {"mp4a.68", MimeUtil::MPEG2_AAC},
    {"mp4a.69", MimeUtil::MP3},
    {"mp4a.6B", MimeUtil::MP3},
    {"mp4a.40.2", MimeUtil::MPEG4_AAC},
    {"mp4a.40.02", MimeUtil::MPEG4_AAC},
    {"mp4a.40.5", MimeUtil::MPEG4_AAC},
    {"mp4a.40.05", MimeUtil::MPEG4_AAC},
    {"mp4a.40.29", MimeUtil::MPEG4_AAC},
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
    // TODO(servolk): Strictly speaking only mp4a.A5 and mp4a.A6 codec ids are
    // valid according to RFC 6381 section 3.3, 3.4. Lower-case oti (mp4a.a5
    // and mp4a.a6) should be rejected. But we used to allow those in older
    // versions of Chromecast firmware and some apps (notably MPL) depend on
    // those codec types being supported, so they should be allowed for now
    // (crbug.com/564960).
    {"ac-3", MimeUtil::AC3},
    {"mp4a.a5", MimeUtil::AC3},
    {"mp4a.A5", MimeUtil::AC3},
    {"ec-3", MimeUtil::EAC3},
    {"mp4a.a6", MimeUtil::EAC3},
    {"mp4a.A6", MimeUtil::EAC3},
#endif
    {"vorbis", MimeUtil::VORBIS},
    {"opus", MimeUtil::OPUS},
    {"flac", MimeUtil::FLAC},
    {"vp8", MimeUtil::VP8},
    {"vp8.0", MimeUtil::VP8},
    {"theora", MimeUtil::THEORA}
  };

  return kStringToCodecMap;
}

static bool ParseVp9CodecID(const std::string& mime_type_lower_case,
                            const std::string& codec_id,
                            VideoCodecProfile* out_profile,
                            uint8_t* out_level) {
  if (mime_type_lower_case == "video/mp4") {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableVp9InMp4)) {
      return ParseNewStyleVp9CodecID(codec_id, out_profile, out_level);
    }
  } else if (mime_type_lower_case == "video/webm") {
    // Only legacy codec strings are supported in WebM.
    // TODO(kqyang): Should we support new codec string in WebM?
    return ParseLegacyVp9CodecID(codec_id, out_profile, out_level);
  }
  return false;
}

static bool IsValidH264Level(uint8_t level_idc) {
  // Valid levels taken from Table A-1 in ISO/IEC 14496-10.
  // Level_idc represents the standard level represented as decimal number
  // multiplied by ten, e.g. level_idc==32 corresponds to level==3.2
  return ((level_idc >= 10 && level_idc <= 13) ||
          (level_idc >= 20 && level_idc <= 22) ||
          (level_idc >= 30 && level_idc <= 32) ||
          (level_idc >= 40 && level_idc <= 42) ||
          (level_idc >= 50 && level_idc <= 51));
}

MimeUtil::MimeUtil() : allow_proprietary_codecs_(false) {
#if defined(OS_ANDROID)
  // When the unified media pipeline is enabled, we need support for both GPU
  // video decoders and MediaCodec; indicated by HasPlatformDecoderSupport().
  // When the Android pipeline is used, we only need access to MediaCodec.
  platform_info_.has_platform_decoders = HasPlatformDecoderSupport();
  platform_info_.has_platform_vp8_decoder =
      MediaCodecUtil::IsVp8DecoderAvailable();
  platform_info_.has_platform_vp9_decoder =
      MediaCodecUtil::IsVp9DecoderAvailable();
  platform_info_.supports_opus = PlatformHasOpusSupport();
#endif

  InitializeMimeTypeMaps();
}

MimeUtil::~MimeUtil() {}

VideoCodec MimeUtilToVideoCodec(MimeUtil::Codec codec) {
  switch (codec) {
    case MimeUtil::H264:
      return kCodecH264;
    case MimeUtil::HEVC:
      return kCodecHEVC;
    case MimeUtil::VP8:
      return kCodecVP8;
    case MimeUtil::VP9:
      return kCodecVP9;
    case MimeUtil::THEORA:
      return kCodecTheora;
    case MimeUtil::DOLBY_VISION:
      return kCodecDolbyVision;
    default:
      break;
  }
  return kUnknownVideoCodec;
}

SupportsType MimeUtil::AreSupportedCodecs(
    const CodecSet& supported_codecs,
    const std::vector<std::string>& codecs,
    const std::string& mime_type_lower_case,
    bool is_encrypted) const {
  DCHECK(!supported_codecs.empty());
  DCHECK(!codecs.empty());
  DCHECK_EQ(base::ToLowerASCII(mime_type_lower_case), mime_type_lower_case);

  SupportsType combined_result = IsSupported;

  for (size_t i = 0; i < codecs.size(); ++i) {
    // Parse the string.
    bool ambiguous_codec_string = false;
    Codec codec = INVALID_CODEC;
    VideoCodecProfile video_profile = VIDEO_CODEC_PROFILE_UNKNOWN;
    uint8_t video_level = 0;
    if (!ParseCodecString(mime_type_lower_case, codecs[i], &codec,
                          &ambiguous_codec_string, &video_profile,
                          &video_level)) {
      return IsNotSupported;
    }

    // Bail if codec not in supported list for given container.
    if (supported_codecs.find(codec) == supported_codecs.end())
      return IsNotSupported;

    // Make conservative guesses to resolve ambiguity before checking platform
    // support. H264 and VP9 are the only allowed ambiguous video codec. DO NOT
    // ADD SUPPORT FOR MORE AMIBIGUOUS STRINGS.
    if (codec == MimeUtil::H264 && ambiguous_codec_string) {
      if (video_profile == VIDEO_CODEC_PROFILE_UNKNOWN)
        video_profile = H264PROFILE_BASELINE;
      if (!IsValidH264Level(video_level))
        video_level = 10;
    } else if (codec == MimeUtil::VP9 && video_level == 0) {
      // Original VP9 content type (codecs="vp9") does not specify the level.
      // TODO(chcunningham): Mark this string as ambiguous when new multi-part
      // VP9 content type is published.
      video_level = 10;
    }

    // Check platform support.
    SupportsType result = IsCodecSupported(
        mime_type_lower_case, codec, video_profile, video_level, is_encrypted);
    if (result == IsNotSupported)
      return IsNotSupported;

    // If any codec is "MayBeSupported", return Maybe for the combined result.
    // Downgrade to MayBeSupported if we had to guess the meaning of one of the
    // codec strings.
    if (result == MayBeSupported ||
        (result == IsSupported && ambiguous_codec_string))
      combined_result = MayBeSupported;
  }

  return combined_result;
}

void MimeUtil::InitializeMimeTypeMaps() {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  allow_proprietary_codecs_ = true;
#endif

  AddSupportedMediaFormats();
}

// Each call to AddContainerWithCodecs() contains a media type
// (https://en.wikipedia.org/wiki/Media_type) and corresponding media codec(s)
// supported by these types/containers.
// TODO(ddorwin): Replace insert() calls with initializer_list when allowed.
void MimeUtil::AddSupportedMediaFormats() {
  CodecSet implicit_codec;
  CodecSet wav_codecs;
  wav_codecs.insert(PCM);

  CodecSet ogg_audio_codecs;
  ogg_audio_codecs.insert(FLAC);
  ogg_audio_codecs.insert(OPUS);
  ogg_audio_codecs.insert(VORBIS);
  CodecSet ogg_video_codecs;
#if !defined(OS_ANDROID)
  ogg_video_codecs.insert(THEORA);
#endif  // !defined(OS_ANDROID)
  CodecSet ogg_codecs(ogg_audio_codecs);
  ogg_codecs.insert(ogg_video_codecs.begin(), ogg_video_codecs.end());

  CodecSet webm_audio_codecs;
  webm_audio_codecs.insert(OPUS);
  webm_audio_codecs.insert(VORBIS);
  CodecSet webm_video_codecs;
  webm_video_codecs.insert(VP8);
  webm_video_codecs.insert(VP9);
  CodecSet webm_codecs(webm_audio_codecs);
  webm_codecs.insert(webm_video_codecs.begin(), webm_video_codecs.end());

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  CodecSet mp3_codecs;
  mp3_codecs.insert(MP3);

  CodecSet aac;
  aac.insert(MPEG2_AAC);
  aac.insert(MPEG4_AAC);

  CodecSet avc_and_aac(aac);
  avc_and_aac.insert(H264);

  CodecSet mp4_audio_codecs(aac);
  mp4_audio_codecs.insert(MP3);
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
  mp4_audio_codecs.insert(AC3);
  mp4_audio_codecs.insert(EAC3);
#endif  // BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)

  CodecSet mp4_video_codecs;
  mp4_video_codecs.insert(H264);
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
  mp4_video_codecs.insert(HEVC);
#endif  // BUILDFLAG(ENABLE_HEVC_DEMUXING)
  // Only VP9 with valid codec string vp09.xx.xx.xx.xx.xx.xx.xx is supported.
  // See ParseVp9CodecID for details.
  mp4_video_codecs.insert(VP9);
#if BUILDFLAG(ENABLE_DOLBY_VISION_DEMUXING)
  mp4_video_codecs.insert(DOLBY_VISION);
#endif  // BUILDFLAG(ENABLE_DOLBY_VISION_DEMUXING)
  CodecSet mp4_codecs(mp4_audio_codecs);
  mp4_codecs.insert(mp4_video_codecs.begin(), mp4_video_codecs.end());
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  AddContainerWithCodecs("audio/wav", wav_codecs, false);
  AddContainerWithCodecs("audio/x-wav", wav_codecs, false);
  AddContainerWithCodecs("audio/webm", webm_audio_codecs, false);
  DCHECK(!webm_video_codecs.empty());
  AddContainerWithCodecs("video/webm", webm_codecs, false);
  AddContainerWithCodecs("audio/ogg", ogg_audio_codecs, false);
  // video/ogg is only supported if an appropriate video codec is supported.
  // Note: This assumes such codecs cannot be later excluded.
  if (!ogg_video_codecs.empty())
    AddContainerWithCodecs("video/ogg", ogg_codecs, false);
  // TODO(ddorwin): Should the application type support Opus?
  AddContainerWithCodecs("application/ogg", ogg_codecs, false);
  AddContainerWithCodecs("audio/flac", implicit_codec, false);

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  AddContainerWithCodecs("audio/mpeg", mp3_codecs, true);  // Allow "mp3".
  AddContainerWithCodecs("audio/mp3", implicit_codec, true);
  AddContainerWithCodecs("audio/x-mp3", implicit_codec, true);
  AddContainerWithCodecs("audio/aac", implicit_codec, true);  // AAC / ADTS.
  AddContainerWithCodecs("audio/mp4", mp4_audio_codecs, true);
  DCHECK(!mp4_video_codecs.empty());
  AddContainerWithCodecs("video/mp4", mp4_codecs, true);
  // These strings are supported for backwards compatibility only and thus only
  // support the codecs needed for compatibility.
  AddContainerWithCodecs("audio/x-m4a", aac, true);
  AddContainerWithCodecs("video/x-m4v", avc_and_aac, true);

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
  // TODO(ddorwin): Exactly which codecs should be supported?
  DCHECK(!mp4_video_codecs.empty());
  AddContainerWithCodecs("video/mp2t", mp4_codecs, true);
#endif  // BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
#if defined(OS_ANDROID)
  // HTTP Live Streaming (HLS).
  // TODO(ddorwin): Is any MP3 codec string variant included in real queries?
  CodecSet hls_codecs(avc_and_aac);
  hls_codecs.insert(MP3);
  AddContainerWithCodecs("application/x-mpegurl", hls_codecs, true);
  AddContainerWithCodecs("application/vnd.apple.mpegurl", hls_codecs, true);
  AddContainerWithCodecs("audio/mpegurl", hls_codecs, true);
  // Not documented by Apple, but unfortunately used extensively by Apple and
  // others for both audio-only and audio+video playlists. See
  // https://crbug.com/675552 for details and examples.
  AddContainerWithCodecs("audio/x-mpegurl", hls_codecs, true);
#endif  // defined(OS_ANDROID)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
}

void MimeUtil::AddContainerWithCodecs(const std::string& mime_type,
                                      const CodecSet& codecs,
                                      bool is_proprietary_mime_type) {
#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  DCHECK(!is_proprietary_mime_type);
#endif

  media_format_map_[mime_type] = codecs;

  if (is_proprietary_mime_type)
    proprietary_media_containers_.push_back(mime_type);
}

bool MimeUtil::IsSupportedMediaMimeType(const std::string& mime_type) const {
  return media_format_map_.find(base::ToLowerASCII(mime_type)) !=
         media_format_map_.end();
}

void MimeUtil::SplitCodecsToVector(const std::string& codecs,
                                   std::vector<std::string>* codecs_out,
                                   bool strip) {
  *codecs_out =
      base::SplitString(base::TrimString(codecs, "\"", base::TRIM_ALL), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Convert empty or all-whitespace input to 0 results.
  if (codecs_out->size() == 1 && (*codecs_out)[0].empty())
    codecs_out->clear();

  if (!strip)
    return;

  // Strip everything past the first '.'
  for (std::vector<std::string>::iterator it = codecs_out->begin();
       it != codecs_out->end(); ++it) {
    size_t found = it->find_first_of('.');
    if (found != std::string::npos)
      it->resize(found);
  }
}

SupportsType MimeUtil::IsSupportedMediaFormat(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    bool is_encrypted) const {
  const std::string mime_type_lower_case = base::ToLowerASCII(mime_type);
  MediaFormatMappings::const_iterator it_media_format_map =
      media_format_map_.find(mime_type_lower_case);
  if (it_media_format_map == media_format_map_.end())
    return IsNotSupported;

  if (it_media_format_map->second.empty()) {
    // We get here if the mimetype does not expect a codecs parameter.
    if (codecs.empty()) {
      return IsDefaultCodecSupported(mime_type_lower_case, is_encrypted);
    } else {
      return IsNotSupported;
    }
  }

  if (codecs.empty()) {
    // We get here if the mimetype expects to get a codecs parameter,
    // but didn't get one. If |mime_type_lower_case| does not have a default
    // codec the best we can do is say "maybe" because we don't have enough
    // information.
    Codec default_codec = INVALID_CODEC;
    if (!GetDefaultCodec(mime_type_lower_case, &default_codec))
      return MayBeSupported;

    return IsSimpleCodecSupported(mime_type_lower_case, default_codec,
                                  is_encrypted);
  }

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
  if (mime_type_lower_case == "video/mp2t") {
    std::vector<std::string> codecs_to_check;
    for (const auto& codec_id : codecs) {
      codecs_to_check.push_back(TranslateLegacyAvc1CodecIds(codec_id));
    }
    return AreSupportedCodecs(it_media_format_map->second, codecs_to_check,
                              mime_type_lower_case, is_encrypted);
  }
#endif

  return AreSupportedCodecs(it_media_format_map->second, codecs,
                            mime_type_lower_case, is_encrypted);
}

void MimeUtil::RemoveProprietaryMediaTypesAndCodecs() {
  for (const auto& container : proprietary_media_containers_)
    media_format_map_.erase(container);
  allow_proprietary_codecs_ = false;
}

// static
bool MimeUtil::IsCodecSupportedOnAndroid(
    Codec codec,
    const std::string& mime_type_lower_case,
    bool is_encrypted,
    const PlatformInfo& platform_info) {
  DCHECK_NE(mime_type_lower_case, "");

  // Encrypted block support is never available without platform decoders.
  if (is_encrypted && !platform_info.has_platform_decoders)
    return false;

  // NOTE: We do not account for Media Source Extensions (MSE) within these
  // checks since it has its own isTypeSupported() which will handle platform
  // specific codec rejections.  See http://crbug.com/587303.

  switch (codec) {
    // ----------------------------------------------------------------------
    // The following codecs are never supported.
    // ----------------------------------------------------------------------
    case INVALID_CODEC:
    case AC3:
    case EAC3:
    case THEORA:
      return false;

    // ----------------------------------------------------------------------
    // The remaining codecs may be supported depending on platform abilities.
    // ----------------------------------------------------------------------

    case PCM:
    case MP3:
    case MPEG4_AAC:
    case FLAC:
    case VORBIS:
      // These codecs are always supported; via a platform decoder (when used
      // with MSE/EME), a software decoder (the unified pipeline), or with
      // MediaPlayer.
      DCHECK(!is_encrypted || platform_info.has_platform_decoders);
      return true;

    case MPEG2_AAC:
      // MPEG-2 variants of AAC are not supported on Android unless the unified
      // media pipeline can be used and the container is not HLS. These codecs
      // will be decoded in software. See https://crbug.com/544268 for details.
      if (base::EndsWith(mime_type_lower_case, "mpegurl",
                         base::CompareCase::SENSITIVE)) {
        return false;
      }
      return !is_encrypted;

    case OPUS:
      // If clear, the unified pipeline can always decode Opus in software.
      if (!is_encrypted)
        return true;

      // Otherwise, platform support is required.
      if (!platform_info.supports_opus)
        return false;

      // MediaPlayer does not support Opus in ogg containers.
      if (base::EndsWith(mime_type_lower_case, "ogg",
                         base::CompareCase::SENSITIVE)) {
        return false;
      }

      DCHECK(!is_encrypted || platform_info.has_platform_decoders);
      return true;

    case H264:
      // When content is not encrypted we fall back to MediaPlayer, thus we
      // always support H264. For EME we need MediaCodec.
      return !is_encrypted || platform_info.has_platform_decoders;

    case HEVC:
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
      if (!platform_info.has_platform_decoders)
        return false;

#if defined(OS_ANDROID)
      // HEVC/H.265 is supported in Lollipop+ (API Level 21), according to
      // http://developer.android.com/reference/android/media/MediaFormat.html
      return base::android::BuildInfo::GetInstance()->sdk_int() >= 21;
#else
      return true;
#endif  // defined(OS_ANDROID)
#else
      return false;
#endif  // BUILDFLAG(ENABLE_HEVC_DEMUXING)

    case VP8:
      // If clear, the unified pipeline can always decode VP8 in software.
      if (!is_encrypted)
        return true;

      if (is_encrypted)
        return platform_info.has_platform_vp8_decoder;

      // MediaPlayer can always play VP8. Note: This is incorrect for MSE, but
      // MSE does not use this code. http://crbug.com/587303.
      return true;

    case VP9: {
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kReportVp9AsAnUnsupportedMimeType)) {
        return false;
      }

      // If clear, the unified pipeline can always decode VP9 in software.
      if (!is_encrypted)
        return true;

      if (!platform_info.has_platform_vp9_decoder)
        return false;

      // Encrypted content is demuxed so the container is irrelevant.
      if (is_encrypted)
        return true;

      // MediaPlayer only supports VP9 in WebM.
      return mime_type_lower_case == "video/webm";
    }

    case DOLBY_VISION:
      // This function is only called on Android which doesn't support Dolby
      // Vision.
      return false;
  }

  return false;
}

bool MimeUtil::ParseCodecString(const std::string& mime_type_lower_case,
                                const std::string& codec_id,
                                Codec* codec,
                                bool* ambiguous_codec_string,
                                VideoCodecProfile* out_profile,
                                uint8_t* out_level) const {
  DCHECK_EQ(base::ToLowerASCII(mime_type_lower_case), mime_type_lower_case);
  DCHECK(codec);
  DCHECK(out_profile);
  DCHECK(out_level);

  *codec = INVALID_CODEC;
  *ambiguous_codec_string = false;
  *out_profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  *out_level = 0;

  std::map<std::string, Codec>::const_iterator itr =
      GetStringToCodecMap().find(codec_id);
  if (itr != GetStringToCodecMap().end()) {
    *codec = itr->second;

    return true;
  }

  // Check codec string against short list of allowed ambiguous codecs.
  // Hard-coded to discourage expansion. DO NOT ADD TO THIS LIST. DO NOT
  // INCREASE PLACES WHERE |ambiguous_codec_string| = true.
  // NOTE: avc1/avc3.XXXXXX may be ambiguous handled after ParseAVCCodecId().
  if (codec_id == "avc1" || codec_id == "avc3") {
    *codec = MimeUtil::H264;
    *ambiguous_codec_string = true;
    return true;
  } else if (codec_id == "mp4a.40") {
    *codec = MimeUtil::MPEG4_AAC;
    *ambiguous_codec_string = true;
    return true;
  }

  // If |codec_id| is not in |kStringToCodecMap|, then we assume that it is
  // either VP9, H.264 or HEVC/H.265 codec ID because currently those are the
  // only ones that are not added to the |kStringToCodecMap| and require
  // parsing.
  if (ParseVp9CodecID(mime_type_lower_case, codec_id, out_profile, out_level)) {
    *codec = MimeUtil::VP9;
    return true;
  }

  if (ParseAVCCodecId(codec_id, out_profile, out_level)) {
    *codec = MimeUtil::H264;
    // Allowed string ambiguity since 2014. DO NOT ADD NEW CASES FOR AMBIGUITY.
    *ambiguous_codec_string = !IsValidH264Level(*out_level);
    return true;
  }

#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
  if (ParseHEVCCodecId(codec_id, out_profile, out_level)) {
    *codec = MimeUtil::HEVC;
    return true;
  }
#endif

#if BUILDFLAG(ENABLE_DOLBY_VISION_DEMUXING)
  if (ParseDolbyVisionCodecId(codec_id, out_profile, out_level)) {
    *codec = MimeUtil::DOLBY_VISION;
    return true;
  }
#endif

  DVLOG(2) << __func__ << ": Unrecognized codec id " << codec_id;
  return false;
}

SupportsType MimeUtil::IsSimpleCodecSupported(
    const std::string& mime_type_lower_case,
    Codec codec,
    bool is_encrypted) const {
  // Video codecs are not "simple" because they require a profile and level to
  // be specified. There is no "default" video codec for a given container.
  DCHECK_EQ(MimeUtilToVideoCodec(codec), kUnknownVideoCodec);

  SupportsType result =
      IsCodecSupported(mime_type_lower_case, codec, VIDEO_CODEC_PROFILE_UNKNOWN,
                       0 /* video_level */, is_encrypted);

  // Platform support should never be ambiguous for simple codecs (no range of
  // profiles to consider).
  DCHECK_NE(result, MayBeSupported);
  return result;
}

SupportsType MimeUtil::IsCodecSupported(const std::string& mime_type_lower_case,
                                        Codec codec,
                                        VideoCodecProfile video_profile,
                                        uint8_t video_level,
                                        bool is_encrypted) const {
  DCHECK_EQ(base::ToLowerASCII(mime_type_lower_case), mime_type_lower_case);
  DCHECK_NE(codec, INVALID_CODEC);

  VideoCodec video_codec = MimeUtilToVideoCodec(codec);
  if (video_codec != kUnknownVideoCodec &&
      // Theora and VP8 do not have profiles/levels.
      video_codec != kCodecTheora && video_codec != kCodecVP8) {
    DCHECK_NE(video_profile, VIDEO_CODEC_PROFILE_UNKNOWN);
    DCHECK_GT(video_level, 0);
  }

  // Bail early for disabled proprietary codecs
  if (!allow_proprietary_codecs_ && IsCodecProprietary(codec)) {
    return IsNotSupported;
  }

  // Check for cases of ambiguous platform support.
  // TODO(chcunningham): DELETE THIS. Platform should know its capabilities.
  // Answer should come from MediaClient.
  bool ambiguous_platform_support = false;
  if (codec == MimeUtil::H264) {
    switch (video_profile) {
      // Always supported
      case H264PROFILE_BASELINE:
      case H264PROFILE_MAIN:
      case H264PROFILE_HIGH:
        break;
// HIGH10PROFILE is supported through fallback to the ffmpeg decoder
// which is not available on Android, or if FFMPEG is not used.
#if !defined(MEDIA_DISABLE_FFMPEG) && !defined(OS_ANDROID)
      case H264PROFILE_HIGH10PROFILE:
        // FFmpeg is not generally used for encrypted videos, so we do not
        // know whether 10-bit is supported.
        ambiguous_platform_support = is_encrypted;
        break;
#endif
      default:
        ambiguous_platform_support = true;
    }
  } else if (codec == MimeUtil::VP9 && video_profile != VP9PROFILE_PROFILE0) {
    // We don't know if the underlying platform supports these profiles. Need
    // to add platform level querying to get supported profiles.
    // https://crbug.com/604566
    ambiguous_platform_support = true;
  }

  if (GetMediaClient() && video_codec != kUnknownVideoCodec &&
      !GetMediaClient()->IsSupportedVideoConfig(video_codec, video_profile,
                                                video_level)) {
    return IsNotSupported;
  }

#if defined(OS_ANDROID)
  // TODO(chcunningham): Delete this. Android platform support should be
  // handled by (android specific) MediaClient.
  if (!IsCodecSupportedOnAndroid(codec, mime_type_lower_case, is_encrypted,
                                 platform_info_)) {
    return IsNotSupported;
  }
#endif

  return ambiguous_platform_support ? MayBeSupported : IsSupported;
}

bool MimeUtil::IsCodecProprietary(Codec codec) const {
  switch (codec) {
    case INVALID_CODEC:
    case AC3:
    case EAC3:
    case MP3:
    case MPEG2_AAC:
    case MPEG4_AAC:
    case H264:
    case HEVC:
    case DOLBY_VISION:
      return true;

    case PCM:
    case VORBIS:
    case OPUS:
    case FLAC:
    case VP8:
    case VP9:
    case THEORA:
      return false;
  }

  return true;
}

bool MimeUtil::GetDefaultCodec(const std::string& mime_type,
                               Codec* default_codec) const {
  if (mime_type == "audio/mpeg" || mime_type == "audio/mp3" ||
      mime_type == "audio/x-mp3") {
    *default_codec = MimeUtil::MP3;
    return true;
  }

  if (mime_type == "audio/aac") {
    *default_codec = MimeUtil::MPEG4_AAC;
    return true;
  }

  if (mime_type == "audio/flac") {
    *default_codec = MimeUtil::FLAC;
    return true;
  }

  return false;
}

SupportsType MimeUtil::IsDefaultCodecSupported(const std::string& mime_type,
                                               bool is_encrypted) const {
  Codec default_codec = Codec::INVALID_CODEC;
  if (!GetDefaultCodec(mime_type, &default_codec))
    return IsNotSupported;
  return IsSimpleCodecSupported(mime_type, default_codec, is_encrypted);
}

}  // namespace internal
}  // namespace media
