// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/offer_messages.h"

#include <inttypes.h>

#include <string>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "cast/streaming/constants.h"
#include "platform/base/error.h"
#include "util/big_endian.h"
#include "util/json/json_reader.h"
#include "util/logging.h"

namespace cast {
namespace streaming {

using openscreen::Error;
using openscreen::ErrorOr;

namespace {

// Cast modes, default is "mirroring"
const char kCastMirroring[] = "mirroring";
const char kCastMode[] = "castMode";
const char kCastRemoting[] = "remoting";

const char kSupportedStreams[] = "supportedStreams";
const char kAudioSourceType[] = "audio_source";
const char kVideoSourceType[] = "video_source";
const char kStreamType[] = "type";

ErrorOr<bool> ParseBool(const Json::Value& value) {
  if (!value.isBool()) {
    return Error::Code::kJsonParseError;
  }
  return value.asBool();
}

ErrorOr<int> ParseInt(const Json::Value& value) {
  if (!value.isInt()) {
    return Error::Code::kJsonParseError;
  }
  return value.asInt();
}

ErrorOr<uint32_t> ParseUint(const Json::Value& value) {
  if (!value.isUInt()) {
    return Error::Code::kJsonParseError;
  }
  return value.asUInt();
}

ErrorOr<std::string> ParseString(const Json::Value& value) {
  if (!value.isString()) {
    return Error::Code::kJsonParseError;
  }
  return value.asString();
}

// Use this template for parsing only when there is a reasonable default
// for the type you are using, e.g. int or std::string.
template <typename T>
T ValueOrDefault(const ErrorOr<T>& value, T fallback = T{}) {
  if (value.is_value()) {
    return value.value();
  }
  return fallback;
}

ErrorOr<RtpPayloadType> ParseRtpPayloadType(const Json::Value& value) {
  auto t = ParseInt(value);
  if (t.is_error()) {
    return t.error();
  }

  uint8_t t_small = t.value();
  if (t_small != t.value() || !IsRtpPayloadType(t_small)) {
    OSP_LOG_ERROR << "Received invalid RTP Payload Type: " << t.value();
    return Error::Code::kParameterInvalid;
  }

  return static_cast<RtpPayloadType>(t_small);
}

ErrorOr<int> ParseRtpTimebase(const Json::Value& value) {
  auto error_or_raw = ParseString(value);
  if (!error_or_raw) {
    return error_or_raw.error();
  }

  int rtp_timebase = 0;
  const char kTimeBasePrefix[] = "1/";
  if (!absl::StartsWith(error_or_raw.value(), kTimeBasePrefix) ||
      !absl::SimpleAtoi(error_or_raw.value().substr(strlen(kTimeBasePrefix)),
                        &rtp_timebase) ||
      (rtp_timebase <= 0)) {
    return Error::Code::kJsonParseError;
  }

  return rtp_timebase;
}

ErrorOr<std::array<uint8_t, 16>> ParseAesHexBytes(const Json::Value& value) {
  auto hex_string = ParseString(value);
  if (!hex_string) {
    return hex_string.error();
  }

  uint64_t quads[2];
  int chars_scanned;
  if (hex_string.value().size() == 32 &&
      sscanf(hex_string.value().c_str(), "%16" SCNx64 "%16" SCNx64 "%n",
             &quads[0], &quads[1], &chars_scanned) == 2 &&
      chars_scanned == 32 &&
      std::none_of(hex_string.value().begin(), hex_string.value().end(),
                   [](char c) { return std::isspace(c); })) {
    std::array<uint8_t, 16> bytes;
    openscreen::WriteBigEndian(quads[0], bytes.data());
    openscreen::WriteBigEndian(quads[1], bytes.data() + 8);
    return bytes;
  }
  return Error::Code::kJsonParseError;
}

ErrorOr<Stream> ParseStream(const Json::Value& value, Stream::Type type) {
  auto index = ParseInt(value["index"]);
  auto codec_name = ParseString(value["codecName"]);
  auto rtp_profile = ParseString(value["rtpProfile"]);
  auto rtp_payload_type = ParseRtpPayloadType(value["rtpPayloadType"]);
  auto ssrc = ParseUint(value["ssrc"]);
  auto target_delay = ParseInt(value["targetDelay"]);
  auto aes_key = ParseAesHexBytes(value["aesKey"]);
  auto aes_iv_mask = ParseAesHexBytes(value["aesIvMask"]);
  auto receiver_rtcp_event_log = ParseBool(value["receiverRtcpEventLog"]);
  auto receiver_rtcp_dscp = ParseString(value["receiverRtcpDscp"]);
  auto rtp_timebase = ParseRtpTimebase(value["timeBase"]);

  if (!index || !codec_name || !rtp_profile || !rtp_payload_type || !ssrc ||
      !rtp_timebase) {
    OSP_LOG_ERROR << "Missing required stream property.";
    return Error::Code::kJsonParseError;
  }

  if (rtp_profile.value() != "cast") {
    OSP_LOG_ERROR << "Received invalid RTP profile: " << rtp_profile;
    return Error::Code::kParameterInvalid;
  }

  if (rtp_timebase.value() <= 0) {
    OSP_LOG_ERROR << "Received invalid RTP timebase: " << rtp_timebase.value();
    return Error::Code::kParameterInvalid;
  }

  std::chrono::milliseconds target_delay_ms = kDefaultTargetPlayoutDelay;
  if (target_delay) {
    auto d = std::chrono::milliseconds(target_delay.value());
    if (d >= kMinTargetPlayoutDelay && d <= kMaxTargetPlayoutDelay) {
      target_delay_ms = d;
    } else {
      OSP_LOG_ERROR << "Sender provided an invalid target delay: " << d.count();
      return Error::Code::kParameterInvalid;
    }
  }

  if (aes_key.is_error() || aes_iv_mask.is_error()) {
    OSP_LOG_ERROR << "Sender did not provide AES Key + IV Mask.";
    return Error::Code::kParameterInvalid;
  }

  return Stream{index.value(),
                type,
                codec_name.value(),
                rtp_payload_type.value(),
                ssrc.value(),
                target_delay_ms,
                aes_key.value(),
                aes_iv_mask.value(),
                ValueOrDefault(receiver_rtcp_event_log),
                ValueOrDefault(receiver_rtcp_dscp),
                rtp_timebase.value()};
}

ErrorOr<AudioStream> ParseAudioStream(const Json::Value& value) {
  auto stream = ParseStream(value, Stream::Type::kAudioSource);
  auto bit_rate = ParseInt(value["bitRate"]);
  auto channels = ParseInt(value["channels"]);

  if (!stream || !bit_rate || !channels) {
    return Error::Code::kJsonParseError;
  }

  if (channels.value() <= 0) {
    OSP_LOG_ERROR << "Received non-positive channel value: "
                  << channels.value();
    return Error::Code::kParameterInvalid;
  }
  if (bit_rate.value() <= 0) {
    OSP_LOG_ERROR << "Received non-positive audio bitrate: "
                  << bit_rate.value();
    return Error::Code::kParameterInvalid;
  }
  return AudioStream{stream.value(), bit_rate.value(), channels.value()};
}

ErrorOr<Resolution> ParseResolution(const Json::Value& value) {
  auto width = ParseInt(value["width"]);
  auto height = ParseInt(value["height"]);

  if (!width || !height) {
    return Error::Code::kJsonParseError;
  }

  if (width <= 0 || height <= 0) {
    OSP_LOG_ERROR << "Received non-positive width or height resolution value";
    return Error::Code::kParameterInvalid;
  }

  return Resolution{width.value(), height.value()};
}

ErrorOr<std::vector<Resolution>> ParseResolutions(const Json::Value& value) {
  std::vector<Resolution> resolutions;
  // Some legacy senders don't provide resolutions, so just return empty.
  if (!value.isArray() || value.empty()) {
    return resolutions;
  }

  for (Json::ArrayIndex i = 0; i < value.size(); ++i) {
    auto r = ParseResolution(value[i]);
    if (!r) {
      return r.error();
    }
    resolutions.push_back(r.value());
  }

  return resolutions;
}

ErrorOr<VideoStream> ParseVideoStream(const Json::Value& value) {
  auto stream = ParseStream(value, Stream::Type::kVideoSource);
  auto max_frame_rate = ParseString(value["maxFrameRate"]);
  auto max_bit_rate = ParseInt(value["maxBitRate"]);
  auto protection = ParseString(value["protection"]);
  auto profile = ParseString(value["profile"]);
  auto level = ParseString(value["level"]);
  auto resolutions = ParseResolutions(value["resolutions"]);
  auto error_recovery_mode = ParseString(value["errorRecoveryMode"]);

  if (!stream || !max_bit_rate || !resolutions) {
    return Error::Code::kJsonParseError;
  }

  double max_frame_rate_value = kDefaultMaxFrameRate;
  if (max_frame_rate.is_value()) {
    // The max frame rate should be given as an fraction composed of natural
    // numbers.
    std::vector<absl::string_view> fields =
        absl::StrSplit(max_frame_rate.value(), '/');
    if (fields.size() == 2) {
      double numerator;
      double denominator;
      if (absl::SimpleAtod(fields[0], &numerator) &&
          absl::SimpleAtod(fields[1], &denominator) && denominator != 0) {
        max_frame_rate_value = numerator / denominator;
      }
    }

    if (!std::isfinite(max_frame_rate_value) || max_frame_rate_value <= 0) {
      OSP_LOG_WARN << "Received invalid max frame rate: '"
                   << max_frame_rate.value() << "'. Using default of: '"
                   << kDefaultMaxFrameRate << "'.";
      max_frame_rate_value = kDefaultMaxFrameRate;
    }
  }

  return VideoStream{stream.value(),
                     max_frame_rate_value,
                     ValueOrDefault(max_bit_rate, 4 << 20),
                     ValueOrDefault(protection),
                     ValueOrDefault(profile),
                     ValueOrDefault(level),
                     resolutions.value(),
                     ValueOrDefault(error_recovery_mode)};
}

ErrorOr<Offer::CastMode> ParseCastMode(const Json::Value& value) {
  auto cast_mode = ParseString(value);
  if (cast_mode.is_error()) {
    OSP_LOG_ERROR << "Received no cast mode";
    return Error::Code::kJsonParseError;
  }

  if (cast_mode.value() == kCastMirroring) {
    return Offer::CastMode::kMirroring;
  }
  if (cast_mode.value() == kCastRemoting) {
    return Offer::CastMode::kRemoting;
  }

  OSP_LOG_ERROR << "Received invalid cast mode: " << cast_mode.value();
  return Error::Code::kJsonParseError;
}
}  // namespace

// static
ErrorOr<Offer> Offer::Parse(const Json::Value& root) {
  auto cast_mode = ParseCastMode(root[kCastMode]);
  if (cast_mode.is_error()) {
    return Error::Code::kJsonParseError;
  }

  Json::Value supported_streams = root[kSupportedStreams];
  if (!supported_streams.isArray()) {
    return Error::Code::kJsonParseError;
  }

  std::vector<AudioStream> audio_streams;
  std::vector<VideoStream> video_streams;
  for (Json::ArrayIndex i = 0; i < supported_streams.size(); ++i) {
    const Json::Value& fields = supported_streams[i];
    auto type = ParseString(fields[kStreamType]);
    if (!type) {
      OSP_LOG_ERROR << "Stream missing mandatory type field.";
      return Error::Code::kJsonParseError;
    }
    if (type.value() == kAudioSourceType) {
      auto stream = ParseAudioStream(fields);
      if (!stream) {
        OSP_LOG_ERROR << "Failed to parse audio stream.";
        return Error::Code::kJsonParseError;
      }
      audio_streams.push_back(std::move(stream.value()));
    } else if (type.value() == kVideoSourceType) {
      auto stream = ParseVideoStream(fields);
      if (!stream) {
        OSP_LOG_ERROR << "Failed to parse video stream.";
        return Error::Code::kJsonParseError;
      }
      video_streams.push_back(std::move(stream.value()));
    }
  }

  return Offer{cast_mode.value(), std::move(audio_streams),
               std::move(video_streams)};
}

}  // namespace streaming
}  // namespace cast