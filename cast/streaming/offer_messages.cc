// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/offer_messages.h"

#include <inttypes.h>

#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "cast/streaming/constants.h"
#include "cast/streaming/message_util.h"
#include "cast/streaming/receiver_session.h"
#include "platform/base/error.h"
#include "util/big_endian.h"
#include "util/json/json_serialization.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {

namespace {

constexpr char kSupportedStreams[] = "supportedStreams";
constexpr char kAudioSourceType[] = "audio_source";
constexpr char kVideoSourceType[] = "video_source";
constexpr char kStreamType[] = "type";

ErrorOr<RtpPayloadType> ParseRtpPayloadType(const Json::Value& parent,
                                            const std::string& field) {
  auto t = ParseInt(parent, field);
  if (!t) {
    return t.error();
  }

  uint8_t t_small = t.value();
  if (t_small != t.value() || !IsRtpPayloadType(t_small)) {
    return Error(Error::Code::kParameterInvalid,
                 "Received invalid RTP Payload Type.");
  }

  return static_cast<RtpPayloadType>(t_small);
}

ErrorOr<int> ParseRtpTimebase(const Json::Value& parent,
                              const std::string& field) {
  auto error_or_raw = ParseString(parent, field);
  if (!error_or_raw) {
    return error_or_raw.error();
  }

  int rtp_timebase = 0;
  constexpr char kTimeBasePrefix[] = "1/";
  if (!absl::StartsWith(error_or_raw.value(), kTimeBasePrefix) ||
      !absl::SimpleAtoi(error_or_raw.value().substr(strlen(kTimeBasePrefix)),
                        &rtp_timebase) ||
      (rtp_timebase <= 0)) {
    return CreateParseError("RTP timebase");
  }

  return rtp_timebase;
}

ErrorOr<std::array<uint8_t, 16>> ParseAesHexBytes(const Json::Value& parent,
                                                  const std::string& field) {
  auto hex_string = ParseString(parent, field);
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
    WriteBigEndian(quads[0], bytes.data());
    WriteBigEndian(quads[1], bytes.data() + 8);
    return bytes;
  }
  return CreateParseError("AES hex string bytes");
}

ErrorOr<Stream> ParseStream(const Json::Value& value, Stream::Type type) {
  auto index = ParseInt(value, "index");
  if (!index) {
    return index.error();
  }
  // If channel is omitted, the default value is used later.
  auto channels = ParseInt(value, "channels");
  if (channels.is_value() && channels.value() <= 0) {
    return CreateParameterError("channel");
  }
  auto codec_name = ParseString(value, "codecName");
  if (!codec_name) {
    return codec_name.error();
  }
  auto rtp_profile = ParseString(value, "rtpProfile");
  if (!rtp_profile) {
    return rtp_profile.error();
  }
  auto rtp_payload_type = ParseRtpPayloadType(value, "rtpPayloadType");
  if (!rtp_payload_type) {
    return rtp_payload_type.error();
  }
  auto ssrc = ParseUint(value, "ssrc");
  if (!ssrc) {
    return ssrc.error();
  }
  auto aes_key = ParseAesHexBytes(value, "aesKey");
  if (!aes_key) {
    return aes_key.error();
  }
  auto aes_iv_mask = ParseAesHexBytes(value, "aesIvMask");
  if (!aes_iv_mask) {
    return aes_iv_mask.error();
  }
  auto rtp_timebase = ParseRtpTimebase(value, "timeBase");
  if (!rtp_timebase) {
    return rtp_timebase.error();
  }

  auto target_delay = ParseInt(value, "targetDelay");
  std::chrono::milliseconds target_delay_ms = kDefaultTargetPlayoutDelay;
  if (target_delay) {
    auto d = std::chrono::milliseconds(target_delay.value());
    if (d >= kMinTargetPlayoutDelay && d <= kMaxTargetPlayoutDelay) {
      target_delay_ms = d;
    } else {
      return CreateParameterError("target delay");
    }
  }

  auto receiver_rtcp_event_log = ParseBool(value, "receiverRtcpEventLog");
  auto receiver_rtcp_dscp = ParseString(value, "receiverRtcpDscp");
  return Stream{index.value(),
                type,
                ValueOrDefault(channels, type == Stream::Type::kAudioSource
                                             ? kDefaultNumAudioChannels
                                             : kDefaultNumVideoChannels),
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
  if (!stream) {
    return stream.error();
  }
  auto bit_rate = ParseInt(value, "bitRate");
  if (!bit_rate) {
    return bit_rate.error();
  }
  if (bit_rate.value() <= 0) {
    return CreateParameterError("bit rate");
  }
  return AudioStream{stream.value(), bit_rate.value()};
}

ErrorOr<Resolution> ParseResolution(const Json::Value& value) {
  auto width = ParseInt(value, "width");
  if (!width) {
    return width.error();
  }
  auto height = ParseInt(value, "height");
  if (!height) {
    return height.error();
  }
  if (width.value() <= 0 || height.value() <= 0) {
    return CreateParameterError("resolution");
  }
  return Resolution{width.value(), height.value()};
}

ErrorOr<std::vector<Resolution>> ParseResolutions(const Json::Value& parent,
                                                  const std::string& field) {
  std::vector<Resolution> resolutions;
  // Some legacy senders don't provide resolutions, so just return empty.
  const Json::Value& value = parent[field];
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
  if (!stream) {
    return stream.error();
  }
  auto resolutions = ParseResolutions(value, "resolutions");
  if (!resolutions) {
    return resolutions.error();
  }

  auto max_frame_rate = ParseString(value, "maxFrameRate");
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

  auto profile = ParseString(value, "profile");
  auto protection = ParseString(value, "protection");
  auto max_bit_rate = ParseInt(value, "maxBitRate");
  auto level = ParseString(value, "level");
  auto error_recovery_mode = ParseString(value, "errorRecoveryMode");
  return VideoStream{stream.value(),
                     max_frame_rate_value,
                     ValueOrDefault(max_bit_rate, 4 << 20),
                     ValueOrDefault(protection),
                     ValueOrDefault(profile),
                     ValueOrDefault(level),
                     resolutions.value(),
                     ValueOrDefault(error_recovery_mode)};
}

}  // namespace

constexpr char kCastMirroring[] = "mirroring";
constexpr char kCastRemoting[] = "remoting";

// static
CastMode CastMode::Parse(absl::string_view value) {
  return (value == kCastRemoting) ? CastMode{CastMode::Type::kRemoting}
                                  : CastMode{CastMode::Type::kMirroring};
}

std::string CastMode::ToString() const {
  switch (type) {
    case Type::kMirroring:
      return kCastMirroring;
    case Type::kRemoting:
      return kCastRemoting;
    default:
      OSP_NOTREACHED();
      return "";
  }
}

// static
ErrorOr<Offer> Offer::Parse(const Json::Value& root) {
  CastMode cast_mode = CastMode::Parse(root["castMode"].asString());

  const ErrorOr<bool> get_status = ParseBool(root, "receiverGetStatus");

  Json::Value supported_streams = root[kSupportedStreams];
  if (!supported_streams.isArray()) {
    return CreateParseError("supported streams in offer");
  }

  std::vector<AudioStream> audio_streams;
  std::vector<VideoStream> video_streams;
  for (Json::ArrayIndex i = 0; i < supported_streams.size(); ++i) {
    const Json::Value& fields = supported_streams[i];
    auto type = ParseString(fields, kStreamType);
    if (!type) {
      return type.error();
    }

    if (type.value() == kAudioSourceType) {
      auto stream = ParseAudioStream(fields);
      if (!stream) {
        return stream.error();
      }
      audio_streams.push_back(std::move(stream.value()));
    } else if (type.value() == kVideoSourceType) {
      auto stream = ParseVideoStream(fields);
      if (!stream) {
        return stream.error();
      }
      video_streams.push_back(std::move(stream.value()));
    }
  }

  return Offer{cast_mode, ValueOrDefault(get_status), std::move(audio_streams),
               std::move(video_streams)};
}

}  // namespace cast
}  // namespace openscreen
