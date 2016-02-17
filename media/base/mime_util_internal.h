// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MIME_UTIL_INTERNAL_H_
#define MEDIA_BASE_MIME_UTIL_INTERNAL_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "media/base/mime_util.h"

namespace media {
namespace internal {

// Internal utility class for handling mime types.  Should only be invoked by
// tests and the functions within mime_util.cc -- NOT for direct use by others.
class MimeUtil {
 public:
  MimeUtil();
  ~MimeUtil();

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

  // See mime_util.h for more information on these methods.
  bool IsSupportedMediaMimeType(const std::string& mime_type) const;
  void ParseCodecString(const std::string& codecs,
                        std::vector<std::string>* codecs_out,
                        bool strip);
  SupportsType IsSupportedMediaFormat(
      const std::string& mime_type,
      const std::vector<std::string>& codecs) const;

  void RemoveProprietaryMediaTypesAndCodecs();

 private:
  typedef base::hash_set<int> CodecSet;
  typedef std::map<std::string, CodecSet> MediaFormatMappings;
  struct CodecEntry {
    CodecEntry() : codec(INVALID_CODEC), is_ambiguous(true) {}
    CodecEntry(Codec c, bool ambiguous) : codec(c), is_ambiguous(ambiguous) {}
    Codec codec;
    bool is_ambiguous;
  };
  typedef std::map<std::string, CodecEntry> StringToCodecMappings;

  // For faster lookup, keep hash sets.
  void InitializeMimeTypeMaps();

  // Returns IsSupported if all codec IDs in |codecs| are unambiguous
  // and are supported by the platform. MayBeSupported is returned if
  // at least one codec ID in |codecs| is ambiguous but all the codecs
  // are supported by the platform. IsNotSupported is returned if at
  // least one codec ID  is not supported by the platform.
  SupportsType AreSupportedCodecs(const CodecSet& supported_codecs,
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

#if defined(OS_ANDROID)
  // Checks special Android only codec restrictions.
  bool IsCodecSupportedOnAndroid(Codec codec) const;
#endif

  // A map of mime_types and hash map of the supported codecs for the mime_type.
  MediaFormatMappings media_format_map_;

  // Keeps track of whether proprietary codec support should be
  // advertised to callers.
  bool allow_proprietary_codecs_;

  // Lookup table for string compare based string -> Codec mappings.
  StringToCodecMappings string_to_codec_map_;

  DISALLOW_COPY_AND_ASSIGN(MimeUtil);
};

}  // namespace internal
}  // namespace media

#endif  // MEDIA_BASE_MIME_UTIL_INTERNAL_H_
