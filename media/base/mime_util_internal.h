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
#include "media/base/media_export.h"
#include "media/base/mime_util.h"
#include "media/base/video_codecs.h"

namespace media {
namespace internal {

// Internal utility class for handling mime types.  Should only be invoked by
// tests and the functions within mime_util.cc -- NOT for direct use by others.
class MEDIA_EXPORT MimeUtil {
 public:
  MimeUtil();
  ~MimeUtil();

  enum Codec {
    INVALID_CODEC,
    PCM,
    MP3,
    AC3,
    EAC3,
    MPEG2_AAC,
    MPEG4_AAC,
    VORBIS,
    OPUS,
    FLAC,
    H264,
    HEVC,
    VP8,
    VP9,
    THEORA,
    DOLBY_VISION,
    LAST_CODEC = DOLBY_VISION
  };

  // Platform configuration structure.  Controls which codecs are supported at
  // runtime.  Also used by tests to simulate platform differences.
  struct PlatformInfo {
    bool has_platform_decoders = false;

    bool has_platform_vp8_decoder = false;
    bool has_platform_vp9_decoder = false;
    bool supports_opus = false;
  };

  // See mime_util.h for more information on these methods.
  bool IsSupportedMediaMimeType(const std::string& mime_type) const;
  void SplitCodecsToVector(const std::string& codecs,
                           std::vector<std::string>* codecs_out,
                           bool strip);
  SupportsType IsSupportedMediaFormat(const std::string& mime_type,
                                      const std::vector<std::string>& codecs,
                                      bool is_encrypted) const;

  void RemoveProprietaryMediaTypesAndCodecs();

  // Checks android platform specific codec restrictions. Returns true if
  // |codec| is supported when contained in |mime_type_lower_case|.
  // |is_encrypted| means the codec will be used with encrypted blocks.
  // |platform_info| describes the availability of various platform features;
  // see PlatformInfo for more details.
  static bool IsCodecSupportedOnAndroid(Codec codec,
                                        const std::string& mime_type_lower_case,
                                        bool is_encrypted,
                                        const PlatformInfo& platform_info);

 private:
  typedef base::hash_set<int> CodecSet;
  typedef std::map<std::string, CodecSet> MediaFormatMappings;

  // Initializes the supported media types into hash sets for faster lookup.
  void InitializeMimeTypeMaps();

  // Initializes the supported media formats (|media_format_map_|).
  void AddSupportedMediaFormats();

  // Adds |mime_type| with the specified codecs to |media_format_map_|.
  void AddContainerWithCodecs(const std::string& mime_type,
                              const CodecSet& codecs_list,
                              bool is_proprietary_mime_type);

  // Returns IsSupported if all codec IDs in |codecs| are unambiguous and are
  // supported in |mime_type_lower_case|. MayBeSupported is returned if at least
  // one codec ID in |codecs| is ambiguous but all the codecs are supported.
  // IsNotSupported is returned if |mime_type_lower_case| is not supported or at
  // least one is not supported in |mime_type_lower_case|. |is_encrypted| means
  // the codec will be used with encrypted blocks.
  SupportsType AreSupportedCodecs(const CodecSet& supported_codecs,
                                  const std::vector<std::string>& codecs,
                                  const std::string& mime_type_lower_case,
                                  bool is_encrypted) const;

  // Converts a codec ID into an Codec enum value and attempts to output the
  // |out_profile| and |out_level|.
  // Returns true if this method was able to map |codec_id| with
  // |mime_type_lower_case| to a specific Codec enum value. |codec| is only
  // valid if true is returned.
  // |ambiguous_codec_string| will be set to true when the codec string matches
  // one of a small number of non-RFC compliant strings (e.g. "avc").
  // |profile| and |level| indicate video codec profile and level (unused for
  // audio codecs). These will be VIDEO_CODEC_PROFILE_UNKNOWN and 0 respectively
  // whenever |codec_id| is incomplete/invalid, or in some cases when
  // |ambiguous_codec_string| is set to true.
  // |is_encrypted| means the codec will be used with encrypted blocks.
  bool ParseCodecString(const std::string& mime_type_lower_case,
                        const std::string& codec_id,
                        Codec* codec,
                        bool* ambiguous_codec_string,
                        VideoCodecProfile* out_profile,
                        uint8_t* out_level) const;

  // Returns IsSupported if |codec| when platform supports codec contained in
  // |mime_type_lower_case|. Returns MayBeSupported when platform support is
  // unclear. Otherwise returns NotSupported. Note: This method will always
  // return NotSupported for proprietary codecs if |allow_proprietary_codecs_|
  // is set to false. |is_encrypted| means the codec will be used with encrypted
  // blocks.
  // TODO(chcunningham): Make this method return a bool. Platform support should
  // always be knowable for a fully specified codec.
  SupportsType IsCodecSupported(const std::string& mime_type_lower_case,
                                Codec codec,
                                VideoCodecProfile video_profile,
                                uint8_t video_level,
                                bool is_encrypted) const;

  // Wrapper around IsCodecSupported for simple codecs that are entirely
  // described (or implied) by the container mime-type.
  SupportsType IsSimpleCodecSupported(const std::string& mime_type_lower_case,
                                      Codec codec,
                                      bool is_encrypted) const;

  // Returns true if |codec| refers to a proprietary codec.
  bool IsCodecProprietary(Codec codec) const;

  // Returns true and sets |*default_codec| if |mime_type_lower_case| has a
  // default codec associated with it. Returns false otherwise and the value of
  // |*default_codec| is undefined.
  bool GetDefaultCodec(const std::string& mime_type_lower_case,
                       Codec* default_codec) const;

  // Returns IsSupported if |mime_type_lower_case| has a default codec
  // associated with it and IsCodecSupported() returns IsSupported for that
  // particular codec. |is_encrypted| means the codec will be used with
  // encrypted blocks.
  SupportsType IsDefaultCodecSupported(const std::string& mime_type_lower_case,
                                       bool is_encrypted) const;

#if defined(OS_ANDROID)
  // Indicates the support of various codecs within the platform.
  PlatformInfo platform_info_;
#endif

  // A map of mime_types and hash map of the supported codecs for the mime_type.
  MediaFormatMappings media_format_map_;

  // List of proprietary containers in |media_format_map_|.
  std::vector<std::string> proprietary_media_containers_;
  // Whether proprietary codec support should be advertised to callers.
  bool allow_proprietary_codecs_;

  DISALLOW_COPY_AND_ASSIGN(MimeUtil);
};

}  // namespace internal
}  // namespace media

#endif  // MEDIA_BASE_MIME_UTIL_INTERNAL_H_
