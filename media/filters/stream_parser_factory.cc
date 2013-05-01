// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/stream_parser_factory.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/webm/webm_stream_parser.h"

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
#include "media/mp4/es_descriptor.h"
#include "media/mp4/mp4_stream_parser.h"
#endif

namespace media {

typedef bool (*CodecIDValidatorFunction)(
    const std::string& codecs_id, const media::LogCB& log_cb);

struct CodecInfo {
  enum Type {
    UNKNOWN,
    AUDIO,
    VIDEO
  };

  const char* pattern;
  Type type;
  CodecIDValidatorFunction validator;
};

typedef media::StreamParser* (*ParserFactoryFunction)(
    const std::vector<std::string>& codecs,
    const media::LogCB& log_cb);

struct SupportedTypeInfo {
  const char* type;
  const ParserFactoryFunction factory_function;
  const CodecInfo** codecs;
};

static const CodecInfo kVP8CodecInfo = { "vp8", CodecInfo::VIDEO, NULL };
static const CodecInfo kVP9CodecInfo = { "vp9", CodecInfo::VIDEO, NULL };
static const CodecInfo kVorbisCodecInfo = { "vorbis", CodecInfo::AUDIO, NULL };

static const CodecInfo* kVideoWebMCodecs[] = {
  &kVP8CodecInfo,
  &kVP9CodecInfo,
  &kVorbisCodecInfo,
  NULL
};

static const CodecInfo* kAudioWebMCodecs[] = {
  &kVorbisCodecInfo,
  NULL
};

static media::StreamParser* BuildWebMParser(
    const std::vector<std::string>& codecs,
    const media::LogCB& log_cb) {
  return new media::WebMStreamParser();
}

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
// AAC Object Type IDs that Chrome supports.
static const int kAACLCObjectType = 2;
static const int kAACSBRObjectType = 5;

static int GetMP4AudioObjectType(const std::string& codec_id,
                                 const media::LogCB& log_cb) {
  int audio_object_type;
  std::vector<std::string> tokens;
  if (Tokenize(codec_id, ".", &tokens) != 3 ||
      tokens[0] != "mp4a" || tokens[1] != "40" ||
      !base::HexStringToInt(tokens[2], &audio_object_type)) {
    MEDIA_LOG(log_cb) << "Malformed mimetype codec '" << codec_id << "'";
    return -1;
  }


  return audio_object_type;
}

bool ValidateMP4ACodecID(const std::string& codec_id,
                         const media::LogCB& log_cb) {
  int audio_object_type = GetMP4AudioObjectType(codec_id, log_cb);
  if (audio_object_type == kAACLCObjectType ||
      audio_object_type == kAACSBRObjectType) {
    return true;
  }

  MEDIA_LOG(log_cb) << "Unsupported audio object type "
                    << "0x" << std::hex << audio_object_type
                    << " in codec '" << codec_id << "'";
  return false;
}

static const CodecInfo kH264CodecInfo = { "avc1.*", CodecInfo::VIDEO, NULL };
static const CodecInfo kMPEG4AACCodecInfo = {
  "mp4a.40.*", CodecInfo::AUDIO, &ValidateMP4ACodecID
};

static const CodecInfo kMPEG2AACLCCodecInfo = {
  "mp4a.67", CodecInfo::AUDIO, NULL
};

#if defined(ENABLE_EAC3_PLAYBACK)
static const CodecInfo kEAC3CodecInfo = {
  "mp4a.a6", CodecInfo::AUDIO, NULL
};
#endif

static const CodecInfo* kVideoMP4Codecs[] = {
  &kH264CodecInfo,
  &kMPEG4AACCodecInfo,
  &kMPEG2AACLCCodecInfo,
  NULL
};

static const CodecInfo* kAudioMP4Codecs[] = {
  &kMPEG4AACCodecInfo,
  &kMPEG2AACLCCodecInfo,
#if defined(ENABLE_EAC3_PLAYBACK)
  &kEAC3CodecInfo,
#endif
  NULL
};

static media::StreamParser* BuildMP4Parser(
    const std::vector<std::string>& codecs, const media::LogCB& log_cb) {
  std::set<int> audio_object_types;
  bool has_sbr = false;
#if defined(ENABLE_EAC3_PLAYBACK)
  bool enable_eac3 = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableEac3Playback);
#endif
  for (size_t i = 0; i < codecs.size(); ++i) {
    std::string codec_id = codecs[i];
    if (MatchPattern(codec_id, kMPEG2AACLCCodecInfo.pattern)) {
      audio_object_types.insert(media::mp4::kISO_13818_7_AAC_LC);
    } else if (MatchPattern(codec_id, kMPEG4AACCodecInfo.pattern)) {
      int audio_object_type = GetMP4AudioObjectType(codec_id, log_cb);
      DCHECK_GT(audio_object_type, 0);

      audio_object_types.insert(media::mp4::kISO_14496_3);

      if (audio_object_type == kAACSBRObjectType) {
        has_sbr = true;
        break;
      }
#if defined(ENABLE_EAC3_PLAYBACK)
    } else if (enable_eac3 && MatchPattern(codec_id, kEAC3CodecInfo.pattern)) {
      audio_object_types.insert(media::mp4::kEAC3);
#endif
    }
  }

  return new media::mp4::MP4StreamParser(audio_object_types, has_sbr);
}
#endif

static const SupportedTypeInfo kSupportedTypeInfo[] = {
  { "video/webm", &BuildWebMParser, kVideoWebMCodecs },
  { "audio/webm", &BuildWebMParser, kAudioWebMCodecs },
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
  { "video/mp4", &BuildMP4Parser, kVideoMP4Codecs },
  { "audio/mp4", &BuildMP4Parser, kAudioMP4Codecs },
#endif
};

// Checks to see if the specified |type| and |codecs| list are supported.
// Returns true if |type| and all codecs listed in |codecs| are supported.
//         |factory_function| contains a function that can build a StreamParser
//                            for this type.
//         |has_audio| is true if an audio codec was specified.
//         |has_video| is true if a video codec was specified.
// Returns false otherwise. The values of |factory_function|, |has_audio|,
//         and |has_video| are undefined.
static bool IsSupported(const std::string& type,
                        const std::vector<std::string>& codecs,
                        const media::LogCB& log_cb,
                        ParserFactoryFunction* factory_function,
                        bool* has_audio,
                        bool* has_video) {
  DCHECK_GT(codecs.size(), 0u);
  *factory_function = NULL;
  *has_audio = false;
  *has_video = false;

  // Search for the SupportedTypeInfo for |type|.
  for (size_t i = 0; i < arraysize(kSupportedTypeInfo); ++i) {
    const SupportedTypeInfo& type_info = kSupportedTypeInfo[i];
    if (type == type_info.type) {
      // Make sure all the codecs specified in |codecs| are
      // in the supported type info.
      for (size_t j = 0; j < codecs.size(); ++j) {
        // Search the type info for a match.
        bool found_codec = false;
        CodecInfo::Type codec_type = CodecInfo::UNKNOWN;
        std::string codec_id = codecs[j];
        for (int k = 0; type_info.codecs[k]; ++k) {
          if (MatchPattern(codec_id, type_info.codecs[k]->pattern) &&
              (!type_info.codecs[k]->validator ||
               type_info.codecs[k]->validator(codec_id, log_cb))) {
            found_codec = true;
            codec_type = type_info.codecs[k]->type;
            break;
          }
        }

        if (!found_codec) {
          MEDIA_LOG(log_cb) << "Codec '" << codec_id
                            <<"' is not supported for '" << type << "'";
          return false;
        }

        const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
        switch (codec_type) {
          case CodecInfo::AUDIO:
#if defined(ENABLE_EAC3_PLAYBACK)
            if (MatchPattern(codec_id, kEAC3CodecInfo.pattern) &&
                !cmd_line->HasSwitch(switches::kEnableEac3Playback)) {
              return false;
            }
#endif
            *has_audio = true;
            break;
          case CodecInfo::VIDEO:
            // TODO(tomfinegan): Remove this check (or negate it, if we just
            // negate the flag) when VP9 is enabled by default.
            if (MatchPattern(codec_id, kVP9CodecInfo.pattern) &&
                !cmd_line->HasSwitch(switches::kEnableVp9Playback)) {
              return false;
            }

            *has_video = true;
            break;
          default:
            MEDIA_LOG(log_cb) << "Unsupported codec type '"<< codec_type
                              << "' for " << codec_id;
            return false;
        }
      }

      *factory_function = type_info.factory_function;

      // All codecs were supported by this |type|.
      return true;
    }
  }

  // |type| didn't match any of the supported types.
  return false;
}

bool StreamParserFactory::IsTypeSupported(
    const std::string& type, const std::vector<std::string>& codecs) {
  bool has_audio;
  bool has_video;
  ParserFactoryFunction factory_function;
  return IsSupported(type, codecs, media::LogCB(), &factory_function,
                     &has_audio, &has_video);
}

scoped_ptr<media::StreamParser> StreamParserFactory::Create(
    const std::string& type, const std::vector<std::string>& codecs,
      const media::LogCB& log_cb, bool* has_audio, bool* has_video) {
  scoped_ptr<media::StreamParser> stream_parser;
  ParserFactoryFunction factory_function;
  if (IsSupported(type, codecs, log_cb, &factory_function,
                  has_audio, has_video)) {
    stream_parser.reset(factory_function(codecs, log_cb));
  }

  return stream_parser.Pass();
}


}  // namespace media
