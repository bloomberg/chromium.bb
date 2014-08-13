// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <map>
#include <string>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "net/base/platform_mime_util.h"
#include "net/http/http_util.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

using std::string;

namespace {

struct MediaType {
  const char name[12];
  const char matcher[13];
};

static const MediaType kIanaMediaTypes[] = {
  { "application", "application/" },
  { "audio", "audio/" },
  { "example", "example/" },
  { "image", "image/" },
  { "message", "message/" },
  { "model", "model/" },
  { "multipart", "multipart/" },
  { "text", "text/" },
  { "video", "video/" },
};

}  // namespace

namespace net {

// Singleton utility class for mime types.
class MimeUtil : public PlatformMimeUtil {
 public:
  enum Codec {
    INVALID_CODEC,
    PCM,
    MP3,
    MPEG2_AAC_LC,
    MPEG2_AAC_MAIN,
    MPEG2_AAC_SSR,
    MPEG4_AAC_LC,
    MPEG4_AAC_SBRv1,
    VORBIS,
    OPUS,
    H264_BASELINE,
    H264_MAIN,
    H264_HIGH,
    VP8,
    VP9,
    THEORA
  };

  bool GetMimeTypeFromExtension(const base::FilePath::StringType& ext,
                                std::string* mime_type) const;

  bool GetMimeTypeFromFile(const base::FilePath& file_path,
                           std::string* mime_type) const;

  bool GetWellKnownMimeTypeFromExtension(const base::FilePath::StringType& ext,
                                         std::string* mime_type) const;

  bool IsSupportedImageMimeType(const std::string& mime_type) const;
  bool IsSupportedMediaMimeType(const std::string& mime_type) const;
  bool IsSupportedNonImageMimeType(const std::string& mime_type) const;
  bool IsUnsupportedTextMimeType(const std::string& mime_type) const;
  bool IsSupportedJavascriptMimeType(const std::string& mime_type) const;

  bool IsSupportedMimeType(const std::string& mime_type) const;

  bool MatchesMimeType(const std::string &mime_type_pattern,
                       const std::string &mime_type) const;

  bool ParseMimeTypeWithoutParameter(const std::string& type_string,
                                     std::string* top_level_type,
                                     std::string* subtype) const;

  bool IsValidTopLevelMimeType(const std::string& type_string) const;

  bool AreSupportedMediaCodecs(const std::vector<std::string>& codecs) const;

  void ParseCodecString(const std::string& codecs,
                        std::vector<std::string>* codecs_out,
                        bool strip);

  bool IsStrictMediaMimeType(const std::string& mime_type) const;
  SupportsType IsSupportedStrictMediaMimeType(
      const std::string& mime_type,
      const std::vector<std::string>& codecs) const;

  void RemoveProprietaryMediaTypesAndCodecsForTests();

 private:
  friend struct base::DefaultLazyInstanceTraits<MimeUtil>;

  typedef base::hash_set<std::string> MimeMappings;

  typedef base::hash_set<int> CodecSet;
  typedef std::map<std::string, CodecSet> StrictMappings;
  struct CodecEntry {
    CodecEntry() : codec(INVALID_CODEC), is_ambiguous(true) {}
    CodecEntry(Codec c, bool ambiguous) : codec(c), is_ambiguous(ambiguous) {}
    Codec codec;
    bool is_ambiguous;
  };
  typedef std::map<std::string, CodecEntry> StringToCodecMappings;

  MimeUtil();

  // Returns IsSupported if all codec IDs in |codecs| are unambiguous
  // and are supported by the platform. MayBeSupported is returned if
  // at least one codec ID in |codecs| is ambiguous but all the codecs
  // are supported by the platform. IsNotSupported is returned if at
  // least one codec ID  is not supported by the platform.
  SupportsType AreSupportedCodecs(
      const CodecSet& supported_codecs,
      const std::vector<std::string>& codecs) const;

  // For faster lookup, keep hash sets.
  void InitializeMimeTypeMaps();

  bool GetMimeTypeFromExtensionHelper(const base::FilePath::StringType& ext,
                                      bool include_platform_types,
                                      std::string* mime_type) const;

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

  // Returns true and sets |*default_codec| if |mime_type| has a
  // default codec associated with it.
  // Returns false otherwise and the value of |*default_codec| is undefined.
  bool GetDefaultCodec(const std::string& mime_type,
                       Codec* default_codec) const;

  // Returns true if |mime_type| has a default codec associated with it
  // and IsCodecSupported() returns true for that particular codec.
  bool IsDefaultCodecSupported(const std::string& mime_type) const;

  MimeMappings image_map_;
  MimeMappings media_map_;
  MimeMappings non_image_map_;
  MimeMappings unsupported_text_map_;
  MimeMappings javascript_map_;

  // A map of mime_types and hash map of the supported codecs for the mime_type.
  StrictMappings strict_format_map_;

  // Keeps track of whether proprietary codec support should be
  // advertised to callers.
  bool allow_proprietary_codecs_;

  // Lookup table for string compare based string -> Codec mappings.
  StringToCodecMappings string_to_codec_map_;
};  // class MimeUtil

// This variable is Leaky because we need to access it from WorkerPool threads.
static base::LazyInstance<MimeUtil>::Leaky g_mime_util =
    LAZY_INSTANCE_INITIALIZER;

struct MimeInfo {
  const char* mime_type;
  const char* extensions;  // comma separated list
};

static const MimeInfo primary_mappings[] = {
  { "text/html", "html,htm,shtml,shtm" },
  { "text/css", "css" },
  { "text/xml", "xml" },
  { "image/gif", "gif" },
  { "image/jpeg", "jpeg,jpg" },
  { "image/webp", "webp" },
  { "image/png", "png" },
  { "video/mp4", "mp4,m4v" },
  { "audio/x-m4a", "m4a" },
  { "audio/mp3", "mp3" },
  { "video/ogg", "ogv,ogm" },
  { "audio/ogg", "ogg,oga,opus" },
  { "video/webm", "webm" },
  { "audio/webm", "webm" },
  { "audio/wav", "wav" },
  { "application/xhtml+xml", "xhtml,xht,xhtm" },
  { "application/x-chrome-extension", "crx" },
  { "multipart/related", "mhtml,mht" }
};

static const MimeInfo secondary_mappings[] = {
  { "application/octet-stream", "exe,com,bin" },
  { "application/gzip", "gz" },
  { "application/pdf", "pdf" },
  { "application/postscript", "ps,eps,ai" },
  { "application/javascript", "js" },
  { "application/font-woff", "woff" },
  { "image/bmp", "bmp" },
  { "image/x-icon", "ico" },
  { "image/vnd.microsoft.icon", "ico" },
  { "image/jpeg", "jfif,pjpeg,pjp" },
  { "image/tiff", "tiff,tif" },
  { "image/x-xbitmap", "xbm" },
  { "image/svg+xml", "svg,svgz" },
  { "image/x-png", "png"},
  { "message/rfc822", "eml" },
  { "text/plain", "txt,text" },
  { "text/html", "ehtml" },
  { "application/rss+xml", "rss" },
  { "application/rdf+xml", "rdf" },
  { "text/xml", "xsl,xbl,xslt" },
  { "application/vnd.mozilla.xul+xml", "xul" },
  { "application/x-shockwave-flash", "swf,swl" },
  { "application/pkcs7-mime", "p7m,p7c,p7z" },
  { "application/pkcs7-signature", "p7s" }
};

static const char* FindMimeType(const MimeInfo* mappings,
                                size_t mappings_len,
                                const char* ext) {
  size_t ext_len = strlen(ext);

  for (size_t i = 0; i < mappings_len; ++i) {
    const char* extensions = mappings[i].extensions;
    for (;;) {
      size_t end_pos = strcspn(extensions, ",");
      if (end_pos == ext_len &&
          base::strncasecmp(extensions, ext, ext_len) == 0)
        return mappings[i].mime_type;
      extensions += end_pos;
      if (!*extensions)
        break;
      extensions += 1;  // skip over comma
    }
  }
  return NULL;
}

bool MimeUtil::GetMimeTypeFromExtension(const base::FilePath::StringType& ext,
                                        string* result) const {
  return GetMimeTypeFromExtensionHelper(ext, true, result);
}

bool MimeUtil::GetWellKnownMimeTypeFromExtension(
    const base::FilePath::StringType& ext,
    string* result) const {
  return GetMimeTypeFromExtensionHelper(ext, false, result);
}

bool MimeUtil::GetMimeTypeFromFile(const base::FilePath& file_path,
                                   string* result) const {
  base::FilePath::StringType file_name_str = file_path.Extension();
  if (file_name_str.empty())
    return false;
  return GetMimeTypeFromExtension(file_name_str.substr(1), result);
}

bool MimeUtil::GetMimeTypeFromExtensionHelper(
    const base::FilePath::StringType& ext,
    bool include_platform_types,
    string* result) const {
  // Avoids crash when unable to handle a long file path. See crbug.com/48733.
  const unsigned kMaxFilePathSize = 65536;
  if (ext.length() > kMaxFilePathSize)
    return false;

  // We implement the same algorithm as Mozilla for mapping a file extension to
  // a mime type.  That is, we first check a hard-coded list (that cannot be
  // overridden), and then if not found there, we defer to the system registry.
  // Finally, we scan a secondary hard-coded list to catch types that we can
  // deduce but that we also want to allow the OS to override.

  base::FilePath path_ext(ext);
  const string ext_narrow_str = path_ext.AsUTF8Unsafe();
  const char* mime_type = FindMimeType(primary_mappings,
                                       arraysize(primary_mappings),
                                       ext_narrow_str.c_str());
  if (mime_type) {
    *result = mime_type;
    return true;
  }

  if (include_platform_types && GetPlatformMimeTypeFromExtension(ext, result))
    return true;

  mime_type = FindMimeType(secondary_mappings, arraysize(secondary_mappings),
                           ext_narrow_str.c_str());
  if (mime_type) {
    *result = mime_type;
    return true;
  }

  return false;
}

// From WebKit's WebCore/platform/MIMETypeRegistry.cpp:

static const char* const supported_image_types[] = {
  "image/jpeg",
  "image/pjpeg",
  "image/jpg",
  "image/webp",
  "image/png",
  "image/gif",
  "image/bmp",
  "image/vnd.microsoft.icon",    // ico
  "image/x-icon",    // ico
  "image/x-xbitmap",  // xbm
  "image/x-png"
};

// A list of media types: http://en.wikipedia.org/wiki/Internet_media_type
// A comprehensive mime type list: http://plugindoc.mozdev.org/winmime.php
// This set of codecs is supported by all variations of Chromium.
static const char* const common_media_types[] = {
  // Ogg.
  "audio/ogg",
  "application/ogg",
#if !defined(OS_ANDROID)  // Android doesn't support Ogg Theora.
  "video/ogg",
#endif

  // WebM.
  "video/webm",
  "audio/webm",

  // Wav.
  "audio/wav",
  "audio/x-wav",

#if defined(OS_ANDROID)
  // HLS. Supported by Android ICS and above.
  "application/vnd.apple.mpegurl",
  "application/x-mpegurl",
#endif
};

// List of proprietary types only supported by Google Chrome.
static const char* const proprietary_media_types[] = {
  // MPEG-4.
  "video/mp4",
  "video/x-m4v",
  "audio/mp4",
  "audio/x-m4a",

  // MP3.
  "audio/mp3",
  "audio/x-mp3",
  "audio/mpeg",
};

// Note:
// - does not include javascript types list (see supported_javascript_types)
// - does not include types starting with "text/" (see
//   IsSupportedNonImageMimeType())
static const char* const supported_non_image_types[] = {
  "image/svg+xml",  // SVG is text-based XML, even though it has an image/ type
  "application/xml",
  "application/atom+xml",
  "application/rss+xml",
  "application/xhtml+xml",
  "application/json",
  "multipart/related",  // For MHTML support.
  "multipart/x-mixed-replace"
  // Note: ADDING a new type here will probably render it AS HTML. This can
  // result in cross site scripting.
};

// Dictionary of cryptographic file mime types.
struct CertificateMimeTypeInfo {
  const char* mime_type;
  CertificateMimeType cert_type;
};

static const CertificateMimeTypeInfo supported_certificate_types[] = {
  { "application/x-x509-user-cert",
      CERTIFICATE_MIME_TYPE_X509_USER_CERT },
#if defined(OS_ANDROID)
  { "application/x-x509-ca-cert", CERTIFICATE_MIME_TYPE_X509_CA_CERT },
  { "application/x-pkcs12", CERTIFICATE_MIME_TYPE_PKCS12_ARCHIVE },
#endif
};

// These types are excluded from the logic that allows all text/ types because
// while they are technically text, it's very unlikely that a user expects to
// see them rendered in text form.
static const char* const unsupported_text_types[] = {
  "text/calendar",
  "text/x-calendar",
  "text/x-vcalendar",
  "text/vcalendar",
  "text/vcard",
  "text/x-vcard",
  "text/directory",
  "text/ldif",
  "text/qif",
  "text/x-qif",
  "text/x-csv",
  "text/x-vcf",
  "text/rtf",
  "text/comma-separated-values",
  "text/csv",
  "text/tab-separated-values",
  "text/tsv",
  "text/ofx",                           // http://crbug.com/162238
  "text/vnd.sun.j2me.app-descriptor"    // http://crbug.com/176450
};

//  Mozilla 1.8 and WinIE 7 both accept text/javascript and text/ecmascript.
//  Mozilla 1.8 accepts application/javascript, application/ecmascript, and
// application/x-javascript, but WinIE 7 doesn't.
//  WinIE 7 accepts text/javascript1.1 - text/javascript1.3, text/jscript, and
// text/livescript, but Mozilla 1.8 doesn't.
//  Mozilla 1.8 allows leading and trailing whitespace, but WinIE 7 doesn't.
//  Mozilla 1.8 and WinIE 7 both accept the empty string, but neither accept a
// whitespace-only string.
//  We want to accept all the values that either of these browsers accept, but
// not other values.
static const char* const supported_javascript_types[] = {
  "text/javascript",
  "text/ecmascript",
  "application/javascript",
  "application/ecmascript",
  "application/x-javascript",
  "text/javascript1.1",
  "text/javascript1.2",
  "text/javascript1.3",
  "text/jscript",
  "text/livescript"
};

#if defined(OS_ANDROID)
static bool IsCodecSupportedOnAndroid(MimeUtil::Codec codec) {
  switch (codec) {
    case MimeUtil::INVALID_CODEC:
      return false;

    case MimeUtil::PCM:
    case MimeUtil::MP3:
    case MimeUtil::MPEG4_AAC_LC:
    case MimeUtil::MPEG4_AAC_SBRv1:
    case MimeUtil::H264_BASELINE:
    case MimeUtil::VP8:
    case MimeUtil::VORBIS:
      return true;

    case MimeUtil::MPEG2_AAC_LC:
    case MimeUtil::MPEG2_AAC_MAIN:
    case MimeUtil::MPEG2_AAC_SSR:
      // MPEG-2 variants of AAC are not supported on Android.
      return false;

    case MimeUtil::H264_MAIN:
    case MimeUtil::H264_HIGH:
      // H.264 Main and High profiles are not supported on Android.
      return false;

    case MimeUtil::VP9:
      // VP9 is supported only in KitKat+ (API Level 19).
      return base::android::BuildInfo::GetInstance()->sdk_int() >= 19;

    case MimeUtil::OPUS:
      // TODO(vigneshv): Change this similar to the VP9 check once Opus is
      // supported on Android (http://crbug.com/318436).
      return false;

    case MimeUtil::THEORA:
      return false;
  }

  return false;
}

static bool IsMimeTypeSupportedOnAndroid(const std::string& mimeType) {
  // HLS codecs are supported in ICS and above (API level 14)
  if ((!mimeType.compare("application/vnd.apple.mpegurl") ||
      !mimeType.compare("application/x-mpegurl")) &&
      base::android::BuildInfo::GetInstance()->sdk_int() < 14) {
    return false;
  }
  return true;
}
#endif

struct MediaFormatStrict {
  const char* mime_type;
  const char* codecs_list;
};

// Following is the list of RFC 6381 compliant codecs:
//   mp4a.66     - MPEG-2 AAC MAIN
//   mp4a.67     - MPEG-2 AAC LC
//   mp4a.68     - MPEG-2 AAC SSR
//   mp4a.69     - MPEG-2 extension to MPEG-1
//   mp4a.6B     - MPEG-1 audio
//   mp4a.40.2   - MPEG-4 AAC LC
//   mp4a.40.5   - MPEG-4 AAC SBRv1
//
//   avc1.42E0xx - H.264 Baseline
//   avc1.4D40xx - H.264 Main
//   avc1.6400xx - H.264 High
static const char kMP4AudioCodecsExpression[] =
    "mp4a.66,mp4a.67,mp4a.68,mp4a.69,mp4a.6B,mp4a.40.2,mp4a.40.5";
static const char kMP4VideoCodecsExpression[] =
    "avc1.42E00A,avc1.4D400A,avc1.64000A," \
    "mp4a.66,mp4a.67,mp4a.68,mp4a.69,mp4a.6B,mp4a.40.2,mp4a.40.5";

static const MediaFormatStrict format_codec_mappings[] = {
  { "video/webm", "opus,vorbis,vp8,vp8.0,vp9,vp9.0" },
  { "audio/webm", "opus,vorbis" },
  { "audio/wav", "1" },
  { "audio/x-wav", "1" },
  { "video/ogg", "opus,theora,vorbis" },
  { "audio/ogg", "opus,vorbis" },
  { "application/ogg", "opus,theora,vorbis" },
  { "audio/mpeg", "mp3" },
  { "audio/mp3", "" },
  { "audio/x-mp3", "" },
  { "audio/mp4", kMP4AudioCodecsExpression },
  { "audio/x-m4a", kMP4AudioCodecsExpression },
  { "video/mp4", kMP4VideoCodecsExpression },
  { "video/x-m4v", kMP4VideoCodecsExpression },
  { "application/x-mpegurl", kMP4VideoCodecsExpression },
  { "application/vnd.apple.mpegurl", kMP4VideoCodecsExpression }
};

struct CodecIDMappings {
  const char* const codec_id;
  MimeUtil::Codec codec;
};

// List of codec IDs that provide enough information to determine the
// codec and profile being requested.
//
// The "mp4a" strings come from RFC 6381.
static const CodecIDMappings kUnambiguousCodecIDs[] = {
  { "1", MimeUtil::PCM }, // We only allow this for WAV so it isn't ambiguous.
  { "mp3", MimeUtil::MP3 },
  { "mp4a.66", MimeUtil::MPEG2_AAC_MAIN },
  { "mp4a.67", MimeUtil::MPEG2_AAC_LC },
  { "mp4a.68", MimeUtil::MPEG2_AAC_SSR },
  { "mp4a.69", MimeUtil::MP3 },
  { "mp4a.6B", MimeUtil::MP3 },
  { "mp4a.40.2", MimeUtil::MPEG4_AAC_LC },
  { "mp4a.40.5", MimeUtil::MPEG4_AAC_SBRv1 },
  { "vorbis", MimeUtil::VORBIS },
  { "opus", MimeUtil::OPUS },
  { "vp8", MimeUtil::VP8 },
  { "vp8.0", MimeUtil::VP8 },
  { "vp9", MimeUtil::VP9 },
  { "vp9.0", MimeUtil::VP9 },
  { "theora", MimeUtil::THEORA }
};

// List of codec IDs that are ambiguous and don't provide
// enough information to determine the codec and profile.
// The codec in these entries indicate the codec and profile
// we assume the user is trying to indicate.
static const CodecIDMappings kAmbiguousCodecIDs[] = {
  { "mp4a.40", MimeUtil::MPEG4_AAC_LC },
  { "avc1", MimeUtil::H264_BASELINE },
  { "avc3", MimeUtil::H264_BASELINE },
};

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
  for (size_t i = 0; i < arraysize(supported_image_types); ++i)
    image_map_.insert(supported_image_types[i]);

  // Initialize the supported non-image types.
  for (size_t i = 0; i < arraysize(supported_non_image_types); ++i)
    non_image_map_.insert(supported_non_image_types[i]);
  for (size_t i = 0; i < arraysize(supported_certificate_types); ++i)
    non_image_map_.insert(supported_certificate_types[i].mime_type);
  for (size_t i = 0; i < arraysize(unsupported_text_types); ++i)
    unsupported_text_map_.insert(unsupported_text_types[i]);
  for (size_t i = 0; i < arraysize(supported_javascript_types); ++i)
    non_image_map_.insert(supported_javascript_types[i]);
  for (size_t i = 0; i < arraysize(common_media_types); ++i) {
#if defined(OS_ANDROID)
    if (!IsMimeTypeSupportedOnAndroid(common_media_types[i]))
      continue;
#endif
    non_image_map_.insert(common_media_types[i]);
  }
#if defined(USE_PROPRIETARY_CODECS)
  allow_proprietary_codecs_ = true;

  for (size_t i = 0; i < arraysize(proprietary_media_types); ++i)
    non_image_map_.insert(proprietary_media_types[i]);
#endif

  // Initialize the supported media types.
  for (size_t i = 0; i < arraysize(common_media_types); ++i) {
#if defined(OS_ANDROID)
    if (!IsMimeTypeSupportedOnAndroid(common_media_types[i]))
      continue;
#endif
    media_map_.insert(common_media_types[i]);
  }
#if defined(USE_PROPRIETARY_CODECS)
  for (size_t i = 0; i < arraysize(proprietary_media_types); ++i)
    media_map_.insert(proprietary_media_types[i]);
#endif

  for (size_t i = 0; i < arraysize(supported_javascript_types); ++i)
    javascript_map_.insert(supported_javascript_types[i]);

  for (size_t i = 0; i < arraysize(kUnambiguousCodecIDs); ++i) {
    string_to_codec_map_[kUnambiguousCodecIDs[i].codec_id] =
        CodecEntry(kUnambiguousCodecIDs[i].codec, false);
  }

  for (size_t i = 0; i < arraysize(kAmbiguousCodecIDs); ++i) {
    string_to_codec_map_[kAmbiguousCodecIDs[i].codec_id] =
        CodecEntry(kAmbiguousCodecIDs[i].codec, true);
  }

  // Initialize the strict supported media types.
  for (size_t i = 0; i < arraysize(format_codec_mappings); ++i) {
    std::vector<std::string> mime_type_codecs;
    ParseCodecString(format_codec_mappings[i].codecs_list,
                     &mime_type_codecs,
                     false);

    CodecSet codecs;
    for (size_t j = 0; j < mime_type_codecs.size(); ++j) {
      Codec codec = INVALID_CODEC;
      bool is_ambiguous = true;
      CHECK(StringToCodec(mime_type_codecs[j], &codec, &is_ambiguous));
      DCHECK(!is_ambiguous);
      codecs.insert(codec);
    }

    strict_format_map_[format_codec_mappings[i].mime_type] = codecs;
  }
}

bool MimeUtil::IsSupportedImageMimeType(const std::string& mime_type) const {
  return image_map_.find(mime_type) != image_map_.end();
}

bool MimeUtil::IsSupportedMediaMimeType(const std::string& mime_type) const {
  return media_map_.find(mime_type) != media_map_.end();
}

bool MimeUtil::IsSupportedNonImageMimeType(const std::string& mime_type) const {
  return non_image_map_.find(mime_type) != non_image_map_.end() ||
      (mime_type.compare(0, 5, "text/") == 0 &&
       !IsUnsupportedTextMimeType(mime_type)) ||
      (mime_type.compare(0, 12, "application/") == 0 &&
       MatchesMimeType("application/*+json", mime_type));
}

bool MimeUtil::IsUnsupportedTextMimeType(const std::string& mime_type) const {
  return unsupported_text_map_.find(mime_type) != unsupported_text_map_.end();
}

bool MimeUtil::IsSupportedJavascriptMimeType(
    const std::string& mime_type) const {
  return javascript_map_.find(mime_type) != javascript_map_.end();
}

// Mirrors WebViewImpl::CanShowMIMEType()
bool MimeUtil::IsSupportedMimeType(const std::string& mime_type) const {
  return (mime_type.compare(0, 6, "image/") == 0 &&
          IsSupportedImageMimeType(mime_type)) ||
         IsSupportedNonImageMimeType(mime_type);
}

// Tests for MIME parameter equality. Each parameter in the |mime_type_pattern|
// must be matched by a parameter in the |mime_type|. If there are no
// parameters in the pattern, the match is a success.
bool MatchesMimeTypeParameters(const std::string& mime_type_pattern,
                               const std::string& mime_type) {
  const std::string::size_type semicolon = mime_type_pattern.find(';');
  const std::string::size_type test_semicolon = mime_type.find(';');
  if (semicolon != std::string::npos) {
    if (test_semicolon == std::string::npos)
      return false;

    std::vector<std::string> pattern_parameters;
    base::SplitString(mime_type_pattern.substr(semicolon + 1),
                      ';', &pattern_parameters);

    std::vector<std::string> test_parameters;
    base::SplitString(mime_type.substr(test_semicolon + 1),
                      ';', &test_parameters);

    sort(pattern_parameters.begin(), pattern_parameters.end());
    sort(test_parameters.begin(), test_parameters.end());
    std::vector<std::string> difference =
      base::STLSetDifference<std::vector<std::string> >(pattern_parameters,
                                                        test_parameters);
    return difference.size() == 0;
  }
  return true;
}

// This comparison handles absolute maching and also basic
// wildcards.  The plugin mime types could be:
//      application/x-foo
//      application/*
//      application/*+xml
//      *
// Also tests mime parameters -- all parameters in the pattern must be present
// in the tested type for a match to succeed.
bool MimeUtil::MatchesMimeType(const std::string& mime_type_pattern,
                               const std::string& mime_type) const {
  // Verify caller is passing lowercase strings.
  DCHECK_EQ(base::StringToLowerASCII(mime_type_pattern), mime_type_pattern);
  DCHECK_EQ(base::StringToLowerASCII(mime_type), mime_type);

  if (mime_type_pattern.empty())
    return false;

  std::string::size_type semicolon = mime_type_pattern.find(';');
  const std::string base_pattern(mime_type_pattern.substr(0, semicolon));
  semicolon = mime_type.find(';');
  const std::string base_type(mime_type.substr(0, semicolon));

  if (base_pattern == "*" || base_pattern == "*/*")
    return MatchesMimeTypeParameters(mime_type_pattern, mime_type);

  const std::string::size_type star = base_pattern.find('*');
  if (star == std::string::npos) {
    if (base_pattern == base_type)
      return MatchesMimeTypeParameters(mime_type_pattern, mime_type);
    else
      return false;
  }

  // Test length to prevent overlap between |left| and |right|.
  if (base_type.length() < base_pattern.length() - 1)
    return false;

  const std::string left(base_pattern.substr(0, star));
  const std::string right(base_pattern.substr(star + 1));

  if (base_type.find(left) != 0)
    return false;

  if (!right.empty() &&
      base_type.rfind(right) != base_type.length() - right.length())
    return false;

  return MatchesMimeTypeParameters(mime_type_pattern, mime_type);
}

// See http://www.iana.org/assignments/media-types/media-types.xhtml
static const char* legal_top_level_types[] = {
  "application",
  "audio",
  "example",
  "image",
  "message",
  "model",
  "multipart",
  "text",
  "video",
};

bool MimeUtil::ParseMimeTypeWithoutParameter(
    const std::string& type_string,
    std::string* top_level_type,
    std::string* subtype) const {
  std::vector<std::string> components;
  base::SplitString(type_string, '/', &components);
  if (components.size() != 2 ||
      !HttpUtil::IsToken(components[0]) ||
      !HttpUtil::IsToken(components[1]))
    return false;

  if (top_level_type)
    *top_level_type = components[0];
  if (subtype)
    *subtype = components[1];
  return true;
}

bool MimeUtil::IsValidTopLevelMimeType(const std::string& type_string) const {
  std::string lower_type = base::StringToLowerASCII(type_string);
  for (size_t i = 0; i < arraysize(legal_top_level_types); ++i) {
    if (lower_type.compare(legal_top_level_types[i]) == 0)
      return true;
  }

  return type_string.size() > 2 && StartsWithASCII(type_string, "x-", false);
}

bool MimeUtil::AreSupportedMediaCodecs(
    const std::vector<std::string>& codecs) const {
  for (size_t i = 0; i < codecs.size(); ++i) {
    Codec codec = INVALID_CODEC;
    bool is_ambiguous = true;
    if (!StringToCodec(codecs[i], &codec, &is_ambiguous) ||
        !IsCodecSupported(codec)) {
      return false;
    }
  }
  return true;
}

void MimeUtil::ParseCodecString(const std::string& codecs,
                                std::vector<std::string>* codecs_out,
                                bool strip) {
  std::string no_quote_codecs;
  base::TrimString(codecs, "\"", &no_quote_codecs);
  base::SplitString(no_quote_codecs, ',', codecs_out);

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

bool MimeUtil::IsStrictMediaMimeType(const std::string& mime_type) const {
  return strict_format_map_.find(mime_type) != strict_format_map_.end();
}

SupportsType MimeUtil::IsSupportedStrictMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs) const {
  StrictMappings::const_iterator it_strict_map =
      strict_format_map_.find(mime_type);
  if (it_strict_map == strict_format_map_.end())
    return codecs.empty() ? MayBeSupported : IsNotSupported;

  if (it_strict_map->second.empty()) {
    // We get here if the mimetype does not expect a codecs parameter.
    return (codecs.empty() && IsDefaultCodecSupported(mime_type)) ?
        IsSupported : IsNotSupported;
  }

  if (codecs.empty()) {
    // We get here if the mimetype expects to get a codecs parameter,
    // but didn't get one. If |mime_type| does not have a default codec
    // the best we can do is say "maybe" because we don't have enough
    // information.
    Codec default_codec = INVALID_CODEC;
    if (!GetDefaultCodec(mime_type, &default_codec))
      return MayBeSupported;

    return IsCodecSupported(default_codec) ? IsSupported : IsNotSupported;
  }

  return AreSupportedCodecs(it_strict_map->second, codecs);
}

void MimeUtil::RemoveProprietaryMediaTypesAndCodecsForTests() {
  for (size_t i = 0; i < arraysize(proprietary_media_types); ++i) {
    non_image_map_.erase(proprietary_media_types[i]);
    media_map_.erase(proprietary_media_types[i]);
  }
  allow_proprietary_codecs_ = false;
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

// Handle parsing H.264 codec IDs as outlined in RFC 6381
//   avc1.42E0xx - H.264 Baseline
//   avc1.4D40xx - H.264 Main
//   avc1.6400xx - H.264 High
//
//   avc1.xxxxxx & avc3.xxxxxx are considered ambiguous forms that
//   are trying to signal H.264 Baseline.
static bool ParseH264CodecID(const std::string& codec_id,
                             MimeUtil::Codec* codec,
                             bool* is_ambiguous) {
  // Make sure we have avc1.xxxxxx or avc3.xxxxxx
  if (codec_id.size() != 11 ||
      (!StartsWithASCII(codec_id, "avc1.", true) &&
       !StartsWithASCII(codec_id, "avc3.", true))) {
    return false;
  }

  std::string profile = StringToUpperASCII(codec_id.substr(5, 4));
  if (profile == "42E0") {
    *codec = MimeUtil::H264_BASELINE;
  } else if (profile == "4D40") {
    *codec = MimeUtil::H264_MAIN;
  } else if (profile == "6400") {
    *codec = MimeUtil::H264_HIGH;
  } else {
    *codec = MimeUtil::H264_BASELINE;
    *is_ambiguous = true;
    return true;
  }

  *is_ambiguous = !IsValidH264Level(StringToUpperASCII(codec_id.substr(9)));
  return true;
}

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
  // an H.264 codec ID because currently those are the only ones that can't be
  // stored in the |string_to_codec_map_| and require parsing.
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
    case MP3:
    case MPEG2_AAC_LC:
    case MPEG2_AAC_MAIN:
    case MPEG2_AAC_SSR:
    case MPEG4_AAC_LC:
    case MPEG4_AAC_SBRv1:
    case H264_BASELINE:
    case H264_MAIN:
    case H264_HIGH:
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

bool MimeUtil::GetDefaultCodec(const std::string& mime_type,
                               Codec* default_codec) const {
  if (mime_type == "audio/mpeg" ||
      mime_type == "audio/mp3" ||
      mime_type == "audio/x-mp3") {
    *default_codec = MimeUtil::MP3;
    return true;
  }

  return false;
}


bool MimeUtil::IsDefaultCodecSupported(const std::string& mime_type) const {
  Codec default_codec = Codec::INVALID_CODEC;
  if (!GetDefaultCodec(mime_type, &default_codec))
    return false;
  return IsCodecSupported(default_codec);
}

//----------------------------------------------------------------------------
// Wrappers for the singleton
//----------------------------------------------------------------------------

bool GetMimeTypeFromExtension(const base::FilePath::StringType& ext,
                              std::string* mime_type) {
  return g_mime_util.Get().GetMimeTypeFromExtension(ext, mime_type);
}

bool GetMimeTypeFromFile(const base::FilePath& file_path,
                         std::string* mime_type) {
  return g_mime_util.Get().GetMimeTypeFromFile(file_path, mime_type);
}

bool GetWellKnownMimeTypeFromExtension(const base::FilePath::StringType& ext,
                                       std::string* mime_type) {
  return g_mime_util.Get().GetWellKnownMimeTypeFromExtension(ext, mime_type);
}

bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      base::FilePath::StringType* extension) {
  return g_mime_util.Get().GetPreferredExtensionForMimeType(mime_type,
                                                            extension);
}

bool IsSupportedImageMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedImageMimeType(mime_type);
}

bool IsSupportedMediaMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedMediaMimeType(mime_type);
}

bool IsSupportedNonImageMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedNonImageMimeType(mime_type);
}

bool IsUnsupportedTextMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsUnsupportedTextMimeType(mime_type);
}

bool IsSupportedJavascriptMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedJavascriptMimeType(mime_type);
}

bool IsSupportedMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsSupportedMimeType(mime_type);
}

bool MatchesMimeType(const std::string& mime_type_pattern,
                     const std::string& mime_type) {
  return g_mime_util.Get().MatchesMimeType(mime_type_pattern, mime_type);
}

bool ParseMimeTypeWithoutParameter(const std::string& type_string,
                                   std::string* top_level_type,
                                   std::string* subtype) {
  return g_mime_util.Get().ParseMimeTypeWithoutParameter(
      type_string, top_level_type, subtype);
}

bool IsValidTopLevelMimeType(const std::string& type_string) {
  return g_mime_util.Get().IsValidTopLevelMimeType(type_string);
}

bool AreSupportedMediaCodecs(const std::vector<std::string>& codecs) {
  return g_mime_util.Get().AreSupportedMediaCodecs(codecs);
}

bool IsStrictMediaMimeType(const std::string& mime_type) {
  return g_mime_util.Get().IsStrictMediaMimeType(mime_type);
}

SupportsType IsSupportedStrictMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs) {
  return g_mime_util.Get().IsSupportedStrictMediaMimeType(mime_type, codecs);
}

void ParseCodecString(const std::string& codecs,
                      std::vector<std::string>* codecs_out,
                      const bool strip) {
  g_mime_util.Get().ParseCodecString(codecs, codecs_out, strip);
}

namespace {

// From http://www.w3schools.com/media/media_mimeref.asp and
// http://plugindoc.mozdev.org/winmime.php
static const char* const kStandardImageTypes[] = {
  "image/bmp",
  "image/cis-cod",
  "image/gif",
  "image/ief",
  "image/jpeg",
  "image/webp",
  "image/pict",
  "image/pipeg",
  "image/png",
  "image/svg+xml",
  "image/tiff",
  "image/vnd.microsoft.icon",
  "image/x-cmu-raster",
  "image/x-cmx",
  "image/x-icon",
  "image/x-portable-anymap",
  "image/x-portable-bitmap",
  "image/x-portable-graymap",
  "image/x-portable-pixmap",
  "image/x-rgb",
  "image/x-xbitmap",
  "image/x-xpixmap",
  "image/x-xwindowdump"
};
static const char* const kStandardAudioTypes[] = {
  "audio/aac",
  "audio/aiff",
  "audio/amr",
  "audio/basic",
  "audio/midi",
  "audio/mp3",
  "audio/mp4",
  "audio/mpeg",
  "audio/mpeg3",
  "audio/ogg",
  "audio/vorbis",
  "audio/wav",
  "audio/webm",
  "audio/x-m4a",
  "audio/x-ms-wma",
  "audio/vnd.rn-realaudio",
  "audio/vnd.wave"
};
static const char* const kStandardVideoTypes[] = {
  "video/avi",
  "video/divx",
  "video/flc",
  "video/mp4",
  "video/mpeg",
  "video/ogg",
  "video/quicktime",
  "video/sd-video",
  "video/webm",
  "video/x-dv",
  "video/x-m4v",
  "video/x-mpeg",
  "video/x-ms-asf",
  "video/x-ms-wmv"
};

struct StandardType {
  const char* leading_mime_type;
  const char* const* standard_types;
  size_t standard_types_len;
};
static const StandardType kStandardTypes[] = {
  { "image/", kStandardImageTypes, arraysize(kStandardImageTypes) },
  { "audio/", kStandardAudioTypes, arraysize(kStandardAudioTypes) },
  { "video/", kStandardVideoTypes, arraysize(kStandardVideoTypes) },
  { NULL, NULL, 0 }
};

void GetExtensionsFromHardCodedMappings(
    const MimeInfo* mappings,
    size_t mappings_len,
    const std::string& leading_mime_type,
    base::hash_set<base::FilePath::StringType>* extensions) {
  base::FilePath::StringType extension;
  for (size_t i = 0; i < mappings_len; ++i) {
    if (StartsWithASCII(mappings[i].mime_type, leading_mime_type, false)) {
      std::vector<string> this_extensions;
      base::SplitString(mappings[i].extensions, ',', &this_extensions);
      for (size_t j = 0; j < this_extensions.size(); ++j) {
#if defined(OS_WIN)
        base::FilePath::StringType extension(
            base::UTF8ToWide(this_extensions[j]));
#else
        base::FilePath::StringType extension(this_extensions[j]);
#endif
        extensions->insert(extension);
      }
    }
  }
}

void GetExtensionsHelper(
    const char* const* standard_types,
    size_t standard_types_len,
    const std::string& leading_mime_type,
    base::hash_set<base::FilePath::StringType>* extensions) {
  for (size_t i = 0; i < standard_types_len; ++i) {
    g_mime_util.Get().GetPlatformExtensionsForMimeType(standard_types[i],
                                                       extensions);
  }

  // Also look up the extensions from hard-coded mappings in case that some
  // supported extensions are not registered in the system registry, like ogg.
  GetExtensionsFromHardCodedMappings(primary_mappings,
                                     arraysize(primary_mappings),
                                     leading_mime_type,
                                     extensions);

  GetExtensionsFromHardCodedMappings(secondary_mappings,
                                     arraysize(secondary_mappings),
                                     leading_mime_type,
                                     extensions);
}

// Note that the elements in the source set will be appended to the target
// vector.
template<class T>
void HashSetToVector(base::hash_set<T>* source, std::vector<T>* target) {
  size_t old_target_size = target->size();
  target->resize(old_target_size + source->size());
  size_t i = 0;
  for (typename base::hash_set<T>::iterator iter = source->begin();
       iter != source->end(); ++iter, ++i)
    (*target)[old_target_size + i] = *iter;
}
}

void GetExtensionsForMimeType(
    const std::string& unsafe_mime_type,
    std::vector<base::FilePath::StringType>* extensions) {
  if (unsafe_mime_type == "*/*" || unsafe_mime_type == "*")
    return;

  const std::string mime_type = base::StringToLowerASCII(unsafe_mime_type);
  base::hash_set<base::FilePath::StringType> unique_extensions;

  if (EndsWith(mime_type, "/*", true)) {
    std::string leading_mime_type = mime_type.substr(0, mime_type.length() - 1);

    // Find the matching StandardType from within kStandardTypes, or fall
    // through to the last (default) StandardType.
    const StandardType* type = NULL;
    for (size_t i = 0; i < arraysize(kStandardTypes); ++i) {
      type = &(kStandardTypes[i]);
      if (type->leading_mime_type &&
          leading_mime_type == type->leading_mime_type)
        break;
    }
    DCHECK(type);
    GetExtensionsHelper(type->standard_types,
                        type->standard_types_len,
                        leading_mime_type,
                        &unique_extensions);
  } else {
    g_mime_util.Get().GetPlatformExtensionsForMimeType(mime_type,
                                                       &unique_extensions);

    // Also look up the extensions from hard-coded mappings in case that some
    // supported extensions are not registered in the system registry, like ogg.
    GetExtensionsFromHardCodedMappings(primary_mappings,
                                       arraysize(primary_mappings),
                                       mime_type,
                                       &unique_extensions);

    GetExtensionsFromHardCodedMappings(secondary_mappings,
                                       arraysize(secondary_mappings),
                                       mime_type,
                                       &unique_extensions);
  }

  HashSetToVector(&unique_extensions, extensions);
}

void RemoveProprietaryMediaTypesAndCodecsForTests() {
  g_mime_util.Get().RemoveProprietaryMediaTypesAndCodecsForTests();
}

const std::string GetIANAMediaType(const std::string& mime_type) {
  for (size_t i = 0; i < arraysize(kIanaMediaTypes); ++i) {
    if (StartsWithASCII(mime_type, kIanaMediaTypes[i].matcher, true)) {
      return kIanaMediaTypes[i].name;
    }
  }
  return std::string();
}

CertificateMimeType GetCertificateMimeTypeForMimeType(
    const std::string& mime_type) {
  // Don't create a map, there is only one entry in the table,
  // except on Android.
  for (size_t i = 0; i < arraysize(supported_certificate_types); ++i) {
    if (mime_type == net::supported_certificate_types[i].mime_type)
      return net::supported_certificate_types[i].cert_type;
  }
  return CERTIFICATE_MIME_TYPE_UNKNOWN;
}

bool IsSupportedCertificateMimeType(const std::string& mime_type) {
  CertificateMimeType file_type =
      GetCertificateMimeTypeForMimeType(mime_type);
  return file_type != CERTIFICATE_MIME_TYPE_UNKNOWN;
}

void AddMultipartValueForUpload(const std::string& value_name,
                                const std::string& value,
                                const std::string& mime_boundary,
                                const std::string& content_type,
                                std::string* post_data) {
  DCHECK(post_data);
  // First line is the boundary.
  post_data->append("--" + mime_boundary + "\r\n");
  // Next line is the Content-disposition.
  post_data->append("Content-Disposition: form-data; name=\"" +
                    value_name + "\"\r\n");
  if (!content_type.empty()) {
    // If Content-type is specified, the next line is that.
    post_data->append("Content-Type: " + content_type + "\r\n");
  }
  // Leave an empty line and append the value.
  post_data->append("\r\n" + value + "\r\n");
}

void AddMultipartFinalDelimiterForUpload(const std::string& mime_boundary,
                                         std::string* post_data) {
  DCHECK(post_data);
  post_data->append("--" + mime_boundary + "--\r\n");
}

}  // namespace net
