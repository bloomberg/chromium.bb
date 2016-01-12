// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/mime_util.h"
#include "media/media_features.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace media {

// Singleton utility class for mime types.
class MimeUtil {
 public:
  enum Codec {
    INVALID_CODEC,
    PCM,
    MP3,
    AC3,
    EAC3,
    MPEG2_AAC_LC,
    MPEG2_AAC_MAIN,
    MPEG2_AAC_SSR,
    MPEG4_AAC_LC,
    MPEG4_AAC_SBR_v1,
    MPEG4_AAC_SBR_PS_v2,
    VORBIS,
    OPUS,
    H264_BASELINE,
    H264_MAIN,
    H264_HIGH,
    HEVC_MAIN,
    VP8,
    VP9,
    THEORA
  };

  bool IsSupportedMediaMimeType(const std::string& mime_type) const;

  void ParseCodecString(const std::string& codecs,
                        std::vector<std::string>* codecs_out,
                        bool strip);

  SupportsType IsSupportedMediaFormat(
      const std::string& mime_type,
      const std::vector<std::string>& codecs) const;

  void RemoveProprietaryMediaTypesAndCodecsForTests();

 private:
  friend struct base::DefaultLazyInstanceTraits<MimeUtil>;

  typedef base::hash_set<int> CodecSet;
  typedef std::map<std::string, CodecSet> MediaFormatMappings;
  struct CodecEntry {
    CodecEntry() : codec(INVALID_CODEC), is_ambiguous(true) {}
    CodecEntry(Codec c, bool ambiguous) : codec(c), is_ambiguous(ambiguous) {}
    Codec codec;
    bool is_ambiguous;
  };
  typedef std::map<std::string, CodecEntry> StringToCodecMappings;

  MimeUtil();

  // For faster lookup, keep hash sets.
  void InitializeMimeTypeMaps();

  // Returns IsSupported if all codec IDs in |codecs| are unambiguous
  // and are supported by the platform. MayBeSupported is returned if
  // at least one codec ID in |codecs| is ambiguous but all the codecs
  // are supported by the platform. IsNotSupported is returned if at
  // least one codec ID  is not supported by the platform.
  SupportsType AreSupportedCodecs(
      const CodecSet& supported_codecs,
      const std::vector<std::string>& codecs) const;

  // Converts a codec ID into an Codec enum value and indicates
  // whether the conversion was ambiguous.
  // Returns true if this method was able to map |codec_id| to a specific
  // Codec enum value. |codec| and |is_ambiguous| are only valid if true
  // is returned. Otherwise their value is undefined after the call.
  // |is_ambiguous| is true if |codec_id| did not have enough information to
  // unambiguously determine the proper Codec enum value. If |is_ambiguous|
  // is true |codec| contains the best guess for the intended Codec enum value.
  bool StringToCodec(const std::string& codec_id,
                     Codec* codec,
                     bool* is_ambiguous) const;

  // Returns true if |codec| is supported by the platform.
  // Note: This method will return false if the platform supports proprietary
  // codecs but |allow_proprietary_codecs_| is set to false.
  bool IsCodecSupported(Codec codec) const;

  // Returns true if |codec| refers to a proprietary codec.
  bool IsCodecProprietary(Codec codec) const;

  // Returns true and sets |*default_codec| if |mime_type| has a  default codec
  // associated with it. Returns false otherwise and the value of
  // |*default_codec| is undefined.
  bool GetDefaultCodecLowerCase(const std::string& mime_type_lower_case,
                                Codec* default_codec) const;

  // Returns true if |mime_type_lower_case| has a default codec associated with
  // it and IsCodecSupported() returns true for that particular codec.
  bool IsDefaultCodecSupportedLowerCase(
      const std::string& mime_type_lower_case) const;

  // A map of mime_types and hash map of the supported codecs for the mime_type.
  MediaFormatMappings media_format_map_;

  // Keeps track of whether proprietary codec support should be
  // advertised to callers.
  bool allow_proprietary_codecs_;

  // Lookup table for string compare based string -> Codec mappings.
  StringToCodecMappings string_to_codec_map_;

  DISALLOW_COPY_AND_ASSIGN(MimeUtil);
};  // class MimeUtil

// This variable is Leaky because it is accessed from WorkerPool threads.
static base::LazyInstance<MimeUtil>::Leaky g_media_mime_util =
    LAZY_INSTANCE_INITIALIZER;

#if defined(OS_ANDROID)
static bool IsCodecSupportedOnAndroid(MimeUtil::Codec codec) {
  switch (codec) {
    case MimeUtil::INVALID_CODEC:
      return false;

    case MimeUtil::PCM:
    case MimeUtil::MP3:
    case MimeUtil::MPEG4_AAC_LC:
    case MimeUtil::MPEG4_AAC_SBR_v1:
    case MimeUtil::MPEG4_AAC_SBR_PS_v2:
    case MimeUtil::VORBIS:
    case MimeUtil::H264_BASELINE:
    case MimeUtil::H264_MAIN:
    case MimeUtil::H264_HIGH:
    case MimeUtil::VP8:
      return true;

    case MimeUtil::AC3:
    case MimeUtil::EAC3:
      // TODO(servolk): Revisit this for AC3/EAC3 support on AndroidTV
      return false;

    case MimeUtil::MPEG2_AAC_LC:
    case MimeUtil::MPEG2_AAC_MAIN:
    case MimeUtil::MPEG2_AAC_SSR:
      // MPEG-2 variants of AAC are not supported on Android.
      return false;

    case MimeUtil::OPUS:
      // Opus is supported only in Lollipop+ (API Level 21).
      return base::android::BuildInfo::GetInstance()->sdk_int() >= 21;

    case MimeUtil::HEVC_MAIN:
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
      // HEVC/H.265 is supported in Lollipop+ (API Level 21), according to
      // http://developer.android.com/reference/android/media/MediaFormat.html
      return base::android::BuildInfo::GetInstance()->sdk_int() >= 21;
#else
      return false;
#endif

    case MimeUtil::VP9:
      // VP9 is supported only in KitKat+ (API Level 19).
      return base::android::BuildInfo::GetInstance()->sdk_int() >= 19;

    case MimeUtil::THEORA:
      return false;
  }

  return false;
}
#endif

enum MediaFormatType { COMMON, PROPRIETARY };

struct MediaFormat {
  const char* const mime_type;
  MediaFormatType format_type;
  const char* const codecs_list;
};

#if defined(USE_PROPRIETARY_CODECS)
// Following is the list of RFC 6381 compliant codecs:
//   mp4a.66     - MPEG-2 AAC MAIN
//   mp4a.67     - MPEG-2 AAC LC
//   mp4a.68     - MPEG-2 AAC SSR
//   mp4a.69     - MPEG-2 extension to MPEG-1
//   mp4a.6B     - MPEG-1 audio
//   mp4a.40.2   - MPEG-4 AAC LC
//   mp4a.40.02  - MPEG-4 AAC LC (leading 0 in aud-oti for compatibility)
//   mp4a.40.5   - MPEG-4 HE-AAC v1 (AAC LC + SBR)
//   mp4a.40.05  - MPEG-4 HE-AAC v1 (AAC LC + SBR) (leading 0 in aud-oti for
//                 compatibility)
//   mp4a.40.29  - MPEG-4 HE-AAC v2 (AAC LC + SBR + PS)
//
//   avc1.42E0xx - H.264 Baseline
//   avc1.4D40xx - H.264 Main
//   avc1.6400xx - H.264 High
static const char kMP4AudioCodecsExpression[] =
    "mp4a.66,mp4a.67,mp4a.68,mp4a.69,mp4a.6B,mp4a.40.2,mp4a.40.02,mp4a.40.5,"
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
    // Only one variant each of ac3 and eac3 codec string is sufficient here,
    // since these strings are parsed and mapped to MimeUtil::Codec enum values.
    "ac-3,ec-3,"
#endif
    "mp4a.40.05,mp4a.40.29";
static const char kMP4VideoCodecsExpression[] =
    // This is not a complete list of supported avc1 codecs. It is simply used
    // to register support for the corresponding Codec enum. Instead of using
    // strings in these three arrays, we should use the Codec enum values.
    // This will avoid confusion and unnecessary parsing at runtime.
    // kUnambiguousCodecStringMap/kAmbiguousCodecStringMap should be the only
    // mapping from strings to codecs. See crbug.com/461009.
    "avc1.42E00A,avc1.4D400A,avc1.64000A,"
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
    // Any valid unambiguous HEVC codec id will work here, since these strings
    // are parsed and mapped to MimeUtil::Codec enum values.
    "hev1.1.6.L93.B0,"
#endif
    "mp4a.66,mp4a.67,mp4a.68,mp4a.69,mp4a.6B,mp4a.40.2,mp4a.40.02,mp4a.40.5,"
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
    // Only one variant each of ac3 and eac3 codec string is sufficient here,
    // since these strings are parsed and mapped to MimeUtil::Codec enum values.
    "ac-3,ec-3,"
#endif
    "mp4a.40.05,mp4a.40.29";
#endif  // USE_PROPRIETARY_CODECS

// A list of media types (https://en.wikipedia.org/wiki/Media_type) and
// corresponding media codecs supported by these types/containers.
// Media formats marked as PROPRIETARY are not supported by Chromium, only
// Google Chrome browser supports them.
static const MediaFormat kFormatCodecMappings[] = {
    {"video/webm", COMMON, "opus,vorbis,vp8,vp8.0,vp9,vp9.0"},
    {"audio/webm", COMMON, "opus,vorbis"},
    {"audio/wav", COMMON, "1"},
    {"audio/x-wav", COMMON, "1"},
#if defined(OS_ANDROID)
    // Android does not support Opus in Ogg container.
    // Android does not support Theora and thus video/ogg.
    {"audio/ogg", COMMON, "vorbis"},
    {"application/ogg", COMMON, "vorbis"},
#else
    {"video/ogg", COMMON, "opus,theora,vorbis"},
    {"audio/ogg", COMMON, "opus,vorbis"},
    {"application/ogg", COMMON, "opus,theora,vorbis"},
#endif
#if defined(USE_PROPRIETARY_CODECS)
    {"audio/mpeg", PROPRIETARY, "mp3"},
    {"audio/mp3", PROPRIETARY, ""},
    {"audio/x-mp3", PROPRIETARY, ""},
    {"audio/aac", PROPRIETARY, ""},  // AAC / ADTS
    {"audio/mp4", PROPRIETARY, kMP4AudioCodecsExpression},
    {"audio/x-m4a", PROPRIETARY, kMP4AudioCodecsExpression},
    {"video/mp4", PROPRIETARY, kMP4VideoCodecsExpression},
    {"video/x-m4v", PROPRIETARY, kMP4VideoCodecsExpression},
#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
    {"video/mp2t", PROPRIETARY, kMP4VideoCodecsExpression},
#endif
#if defined(OS_ANDROID)
    // HTTP Live Streaming (HLS)
    {"application/x-mpegurl", PROPRIETARY, kMP4VideoCodecsExpression},
    {"application/vnd.apple.mpegurl", PROPRIETARY, kMP4VideoCodecsExpression}
#endif
#endif  // USE_PROPRIETARY_CODECS
};

struct CodecIDMappings {
  const char* const codec_id;
  MimeUtil::Codec codec;
};

// List of codec IDs that provide enough information to determine the
// codec and profile being requested.
//
// The "mp4a" strings come from RFC 6381.
static const CodecIDMappings kUnambiguousCodecStringMap[] = {
    {"1", MimeUtil::PCM},  // We only allow this for WAV so it isn't ambiguous.
    // avc1/avc3.XXXXXX may be unambiguous; handled by ParseH264CodecID().
    // hev1/hvc1.XXXXXX may be unambiguous; handled by ParseHEVCCodecID().
    {"mp3", MimeUtil::MP3},
    {"mp4a.66", MimeUtil::MPEG2_AAC_MAIN},
    {"mp4a.67", MimeUtil::MPEG2_AAC_LC},
    {"mp4a.68", MimeUtil::MPEG2_AAC_SSR},
    {"mp4a.69", MimeUtil::MP3},
    {"mp4a.6B", MimeUtil::MP3},
    {"mp4a.40.2", MimeUtil::MPEG4_AAC_LC},
    {"mp4a.40.02", MimeUtil::MPEG4_AAC_LC},
    {"mp4a.40.5", MimeUtil::MPEG4_AAC_SBR_v1},
    {"mp4a.40.05", MimeUtil::MPEG4_AAC_SBR_v1},
    {"mp4a.40.29", MimeUtil::MPEG4_AAC_SBR_PS_v2},
#if BUILDFLAG(ENABLE_AC3_EAC3_AUDIO_DEMUXING)
    // TODO(servolk): Strictly speaking only mp4a.A5 and mp4a.A6 codec ids are
    // valid according to RFC 6381 section 3.3, 3.4. Lower-case oti (mp4a.a5 and
    // mp4a.a6) should be rejected. But we used to allow those in older versions
    // of Chromecast firmware and some apps (notably MPL) depend on those codec
    // types being supported, so they should be allowed for now
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
    {"vp8", MimeUtil::VP8},
    {"vp8.0", MimeUtil::VP8},
    {"vp9", MimeUtil::VP9},
    {"vp9.0", MimeUtil::VP9},
    {"theora", MimeUtil::THEORA}};

// List of codec IDs that are ambiguous and don't provide
// enough information to determine the codec and profile.
// The codec in these entries indicate the codec and profile
// we assume the user is trying to indicate.
static const CodecIDMappings kAmbiguousCodecStringMap[] = {
    {"mp4a.40", MimeUtil::MPEG4_AAC_LC},
    {"avc1", MimeUtil::H264_BASELINE},
    {"avc3", MimeUtil::H264_BASELINE},
    // avc1/avc3.XXXXXX may be ambiguous; handled by ParseH264CodecID().
};

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
static const char kHexString[] = "0123456789ABCDEF";
static char IntToHex(int i) {
  DCHECK_GE(i, 0) << i << " not a hex value";
  DCHECK_LE(i, 15) << i << " not a hex value";
  return kHexString[i];
}

std::string TranslateLegacyAvc1CodecIds(const std::string& codec_id) {
  // Special handling for old, pre-RFC 6381 format avc1 strings, which are still
  // being used by some HLS apps to preserve backward compatibility with older
  // iOS devices. The old format was avc1.<profile>.<level>
  // Where <profile> is H.264 profile_idc encoded as a decimal number, i.e.
  // 66 is baseline profile (0x42)
  // 77 is main profile (0x4d)
  // 100 is high profile (0x64)
  // And <level> is H.264 level multiplied by 10, also encoded as decimal number
  // E.g. <level> 31 corresponds to H.264 level 3.1
  // See, for example, http://qtdevseed.apple.com/qadrift/testcases/tc-0133.php
  uint32_t level_start = 0;
  std::string result;
  if (base::StartsWith(codec_id, "avc1.66.", base::CompareCase::SENSITIVE)) {
    level_start = 8;
    result = "avc1.4200";
  } else if (base::StartsWith(codec_id, "avc1.77.",
                              base::CompareCase::SENSITIVE)) {
    level_start = 8;
    result = "avc1.4D00";
  } else if (base::StartsWith(codec_id, "avc1.100.",
                              base::CompareCase::SENSITIVE)) {
    level_start = 9;
    result = "avc1.6400";
  }

  uint32_t level = 0;
  if (level_start > 0 &&
      base::StringToUint(codec_id.substr(level_start), &level) && level < 256) {
    // This is a valid legacy avc1 codec id - return the codec id translated
    // into RFC 6381 format.
    result.push_back(IntToHex(level >> 4));
    result.push_back(IntToHex(level & 0xf));
    return result;
  }

  // This is not a valid legacy avc1 codec id - return the original codec id.
  return codec_id;
}
#endif

MimeUtil::MimeUtil() : allow_proprietary_codecs_(false) {
  InitializeMimeTypeMaps();
}

SupportsType MimeUtil::AreSupportedCodecs(
    const CodecSet& supported_codecs,
    const std::vector<std::string>& codecs) const {
  DCHECK(!supported_codecs.empty());
  DCHECK(!codecs.empty());

  SupportsType result = IsSupported;
  for (size_t i = 0; i < codecs.size(); ++i) {
    bool is_ambiguous = true;
    Codec codec = INVALID_CODEC;
    if (!StringToCodec(codecs[i], &codec, &is_ambiguous))
      return IsNotSupported;

    if (!IsCodecSupported(codec) ||
        supported_codecs.find(codec) == supported_codecs.end()) {
      return IsNotSupported;
    }

    if (is_ambiguous)
      result = MayBeSupported;
  }

  return result;
}

void MimeUtil::InitializeMimeTypeMaps() {
  // Initialize the supported media types.
#if defined(USE_PROPRIETARY_CODECS)
  allow_proprietary_codecs_ = true;
#endif

  for (size_t i = 0; i < arraysize(kUnambiguousCodecStringMap); ++i) {
    string_to_codec_map_[kUnambiguousCodecStringMap[i].codec_id] =
        CodecEntry(kUnambiguousCodecStringMap[i].codec, false);
  }

  for (size_t i = 0; i < arraysize(kAmbiguousCodecStringMap); ++i) {
    string_to_codec_map_[kAmbiguousCodecStringMap[i].codec_id] =
        CodecEntry(kAmbiguousCodecStringMap[i].codec, true);
  }

  // Initialize the supported media formats.
  for (size_t i = 0; i < arraysize(kFormatCodecMappings); ++i) {
    std::vector<std::string> mime_type_codecs;
    ParseCodecString(kFormatCodecMappings[i].codecs_list, &mime_type_codecs,
                     false);

    CodecSet codecs;
    for (size_t j = 0; j < mime_type_codecs.size(); ++j) {
      Codec codec = INVALID_CODEC;
      bool is_ambiguous = true;
      CHECK(StringToCodec(mime_type_codecs[j], &codec, &is_ambiguous));
      DCHECK(!is_ambiguous);
      codecs.insert(codec);
    }

    media_format_map_[kFormatCodecMappings[i].mime_type] = codecs;
  }
}

bool MimeUtil::IsSupportedMediaMimeType(const std::string& mime_type) const {
  return media_format_map_.find(base::ToLowerASCII(mime_type)) !=
         media_format_map_.end();
}

void MimeUtil::ParseCodecString(const std::string& codecs,
                                std::vector<std::string>* codecs_out,
                                bool strip) {
  *codecs_out = base::SplitString(
      base::TrimString(codecs, "\"", base::TRIM_ALL),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Convert empty or all-whitespace input to 0 results.
  if (codecs_out->size() == 1 && (*codecs_out)[0].empty())
    codecs_out->clear();

  if (!strip)
    return;

  // Strip everything past the first '.'
  for (std::vector<std::string>::iterator it = codecs_out->begin();
       it != codecs_out->end();
       ++it) {
    size_t found = it->find_first_of('.');
    if (found != std::string::npos)
      it->resize(found);
  }
}

SupportsType MimeUtil::IsSupportedMediaFormat(
    const std::string& mime_type,
    const std::vector<std::string>& codecs) const {
  const std::string mime_type_lower_case = base::ToLowerASCII(mime_type);
  MediaFormatMappings::const_iterator it_media_format_map =
      media_format_map_.find(mime_type_lower_case);
  if (it_media_format_map == media_format_map_.end())
    return IsNotSupported;

  if (it_media_format_map->second.empty()) {
    // We get here if the mimetype does not expect a codecs parameter.
    return (codecs.empty() &&
            IsDefaultCodecSupportedLowerCase(mime_type_lower_case))
               ? IsSupported
               : IsNotSupported;
  }

  if (codecs.empty()) {
    // We get here if the mimetype expects to get a codecs parameter,
    // but didn't get one. If |mime_type_lower_case| does not have a default
    // codec the best we can do is say "maybe" because we don't have enough
    // information.
    Codec default_codec = INVALID_CODEC;
    if (!GetDefaultCodecLowerCase(mime_type_lower_case, &default_codec))
      return MayBeSupported;

    return IsCodecSupported(default_codec) ? IsSupported : IsNotSupported;
  }

#if BUILDFLAG(ENABLE_MSE_MPEG2TS_STREAM_PARSER)
  if (mime_type_lower_case == "video/mp2t") {
    std::vector<std::string> codecs_to_check;
    for (const auto& codec_id : codecs) {
      codecs_to_check.push_back(TranslateLegacyAvc1CodecIds(codec_id));
    }
    return AreSupportedCodecs(it_media_format_map->second, codecs_to_check);
  }
#endif

  return AreSupportedCodecs(it_media_format_map->second, codecs);
}

void MimeUtil::RemoveProprietaryMediaTypesAndCodecsForTests() {
  for (size_t i = 0; i < arraysize(kFormatCodecMappings); ++i)
    if (kFormatCodecMappings[i].format_type == PROPRIETARY)
      media_format_map_.erase(kFormatCodecMappings[i].mime_type);
  allow_proprietary_codecs_ = false;
}

static bool IsValidH264Level(const std::string& level_str) {
  uint32_t level;
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

// Handle parsing H.264 codec IDs as outlined in RFC 6381 and ISO-14496-10.
//   avc1.42x0yy - H.264 Baseline
//   avc1.4Dx0yy - H.264 Main
//   avc1.64x0yy - H.264 High
//
//   avc1.xxxxxx & avc3.xxxxxx are considered ambiguous forms that are trying to
//   signal H.264 Baseline. For example, the idc_level, profile_idc and
//   constraint_set3_flag pieces may explicitly require decoder to conform to
//   baseline profile at the specified level (see Annex A and constraint_set0 in
//   ISO-14496-10).
static bool ParseH264CodecID(const std::string& codec_id,
                             MimeUtil::Codec* codec,
                             bool* is_ambiguous) {
  // Make sure we have avc1.xxxxxx or avc3.xxxxxx , where xxxxxx are hex digits
  if (!base::StartsWith(codec_id, "avc1.", base::CompareCase::SENSITIVE) &&
      !base::StartsWith(codec_id, "avc3.", base::CompareCase::SENSITIVE)) {
    return false;
  }
  if (codec_id.size() != 11 ||
      !base::IsHexDigit(codec_id[5]) || !base::IsHexDigit(codec_id[6]) ||
      !base::IsHexDigit(codec_id[7]) || !base::IsHexDigit(codec_id[8]) ||
      !base::IsHexDigit(codec_id[9]) || !base::IsHexDigit(codec_id[10])) {
    return false;
  }

  // Validate constraint flags and reserved bits.
  if (!base::IsHexDigit(codec_id[7]) || codec_id[8] != '0') {
    *codec = MimeUtil::H264_BASELINE;
    *is_ambiguous = true;
    return true;
  }

  // Extract the profile.
  std::string profile = base::ToUpperASCII(codec_id.substr(5, 2));
  if (profile == "42") {
    *codec = MimeUtil::H264_BASELINE;
  } else if (profile == "4D") {
    *codec = MimeUtil::H264_MAIN;
  } else if (profile == "64") {
    *codec = MimeUtil::H264_HIGH;
  } else {
    *codec = MimeUtil::H264_BASELINE;
    *is_ambiguous = true;
    return true;
  }

  // Validate level.
  *is_ambiguous = !IsValidH264Level(codec_id.substr(9));
  return true;
}

#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
// ISO/IEC FDIS 14496-15 standard section E.3 describes the syntax of codec ids
// reserved for HEVC. According to that spec HEVC codec id must start with
// either "hev1." or "hvc1.". We don't yet support full parsing of HEVC codec
// ids, but since no other codec id starts with those string we'll just treat
// any string starting with "hev1." or "hvc1." as valid HEVC codec ids.
// crbug.com/482761
static bool ParseHEVCCodecID(const std::string& codec_id,
                             MimeUtil::Codec* codec,
                             bool* is_ambiguous) {
  if (base::StartsWith(codec_id, "hev1.", base::CompareCase::SENSITIVE) ||
      base::StartsWith(codec_id, "hvc1.", base::CompareCase::SENSITIVE)) {
    *codec = MimeUtil::HEVC_MAIN;

    // TODO(servolk): Full HEVC codec id parsing is not implemented yet (see
    // crbug.com/482761). So treat HEVC codec ids as ambiguous for now.
    *is_ambiguous = true;

    // TODO(servolk): Most HEVC codec ids are treated as ambiguous (see above),
    // but we need to recognize at least one valid unambiguous HEVC codec id,
    // which is added into kMP4VideoCodecsExpression. We need it to be
    // unambiguous to avoid DCHECK(!is_ambiguous) in InitializeMimeTypeMaps. We
    // also use these in unit tests (see
    // content/browser/media/media_canplaytype_browsertest.cc).
    // Remove this workaround after crbug.com/482761 is fixed.
    if (codec_id == "hev1.1.6.L93.B0" || codec_id == "hvc1.1.6.L93.B0") {
      *is_ambiguous = false;
    }

    return true;
  }

  return false;
}
#endif

bool MimeUtil::StringToCodec(const std::string& codec_id,
                             Codec* codec,
                             bool* is_ambiguous) const {
  StringToCodecMappings::const_iterator itr =
      string_to_codec_map_.find(codec_id);
  if (itr != string_to_codec_map_.end()) {
    *codec = itr->second.codec;
    *is_ambiguous = itr->second.is_ambiguous;
    return true;
  }

  // If |codec_id| is not in |string_to_codec_map_|, then we assume that it is
  // either H.264 or HEVC/H.265 codec ID because currently those are the only
  // ones that are not added to the |string_to_codec_map_| and require parsing.
#if BUILDFLAG(ENABLE_HEVC_DEMUXING)
  if (ParseHEVCCodecID(codec_id, codec, is_ambiguous)) {
    return true;
  }
#endif
  return ParseH264CodecID(codec_id, codec, is_ambiguous);
}

bool MimeUtil::IsCodecSupported(Codec codec) const {
  DCHECK_NE(codec, INVALID_CODEC);

#if defined(OS_ANDROID)
  if (!IsCodecSupportedOnAndroid(codec))
    return false;
#endif

  return allow_proprietary_codecs_ || !IsCodecProprietary(codec);
}

bool MimeUtil::IsCodecProprietary(Codec codec) const {
  switch (codec) {
    case INVALID_CODEC:
    case AC3:
    case EAC3:
    case MP3:
    case MPEG2_AAC_LC:
    case MPEG2_AAC_MAIN:
    case MPEG2_AAC_SSR:
    case MPEG4_AAC_LC:
    case MPEG4_AAC_SBR_v1:
    case MPEG4_AAC_SBR_PS_v2:
    case H264_BASELINE:
    case H264_MAIN:
    case H264_HIGH:
    case HEVC_MAIN:
      return true;

    case PCM:
    case VORBIS:
    case OPUS:
    case VP8:
    case VP9:
    case THEORA:
      return false;
  }

  return true;
}

bool MimeUtil::GetDefaultCodecLowerCase(const std::string& mime_type_lower_case,
                                        Codec* default_codec) const {
  if (mime_type_lower_case == "audio/mpeg" ||
      mime_type_lower_case == "audio/mp3" ||
      mime_type_lower_case == "audio/x-mp3") {
    *default_codec = MimeUtil::MP3;
    return true;
  }

  if (mime_type_lower_case == "audio/aac") {
    *default_codec = MimeUtil::MPEG4_AAC_LC;
    return true;
  }

  return false;
}

bool MimeUtil::IsDefaultCodecSupportedLowerCase(
    const std::string& mime_type_lower_case) const {
  Codec default_codec = Codec::INVALID_CODEC;
  if (!GetDefaultCodecLowerCase(mime_type_lower_case, &default_codec))
    return false;
  return IsCodecSupported(default_codec);
}

bool IsSupportedMediaMimeType(const std::string& mime_type) {
  return g_media_mime_util.Get().IsSupportedMediaMimeType(mime_type);
}

SupportsType IsSupportedMediaFormat(const std::string& mime_type,
                                    const std::vector<std::string>& codecs) {
  return g_media_mime_util.Get().IsSupportedMediaFormat(mime_type, codecs);
}

void ParseCodecString(const std::string& codecs,
                      std::vector<std::string>* codecs_out,
                      const bool strip) {
  g_media_mime_util.Get().ParseCodecString(codecs, codecs_out, strip);
}

void RemoveProprietaryMediaTypesAndCodecsForTests() {
  g_media_mime_util.Get().RemoveProprietaryMediaTypesAndCodecsForTests();
}

}  // namespace media
