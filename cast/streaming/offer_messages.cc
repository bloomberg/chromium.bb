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
#include "util/stringprintf.h"

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

  const auto fraction = SimpleFraction::FromString(error_or_raw.value());
  if (fraction.is_error() || !fraction.value().is_positive()) {
    return CreateParseError("RTP timebase");
  }
  // The spec demands a leading 1, so this isn't really a fraction.
  OSP_DCHECK(fraction.value().numerator == 1);
  return fraction.value().denominator;
}

// For a hex byte, the conversion is 4 bits to 1 character, e.g.
// 0b11110001 becomes F1, so 1 byte is two characters.
constexpr int kHexDigitsPerByte = 2;
constexpr int kAesBytesSize = 16;
constexpr int kAesStringLength = kAesBytesSize * kHexDigitsPerByte;
ErrorOr<std::array<uint8_t, kAesBytesSize>> ParseAesHexBytes(
    const Json::Value& parent,
    const std::string& field) {
  auto hex_string = ParseString(parent, field);
  if (!hex_string) {
    return hex_string.error();
  }

  constexpr int kHexDigitsPerScanField = 16;
  constexpr int kNumScanFields = kAesStringLength / kHexDigitsPerScanField;
  uint64_t quads[kNumScanFields];
  int chars_scanned;
  if (hex_string.value().size() == kAesStringLength &&
      sscanf(hex_string.value().c_str(), "%16" SCNx64 "%16" SCNx64 "%n",
             &quads[0], &quads[1], &chars_scanned) == kNumScanFields &&
      chars_scanned == kAesStringLength &&
      std::none_of(hex_string.value().begin(), hex_string.value().end(),
                   [](char c) { return std::isspace(c); })) {
    std::array<uint8_t, kAesBytesSize> bytes;
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
  // A bit rate of 0 is valid for some codec types, so we don't enforce here.
  if (bit_rate.value() < 0) {
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

  auto raw_max_frame_rate = ParseString(value, "maxFrameRate");
  SimpleFraction max_frame_rate{kDefaultMaxFrameRate, 1};
  if (raw_max_frame_rate.is_value()) {
    auto parsed = SimpleFraction::FromString(raw_max_frame_rate.value());
    if (parsed.is_value() && parsed.value().is_positive()) {
      max_frame_rate = parsed.value();
    }
  }

  auto profile = ParseString(value, "profile");
  auto protection = ParseString(value, "protection");
  auto max_bit_rate = ParseInt(value, "maxBitRate");
  auto level = ParseString(value, "level");
  auto error_recovery_mode = ParseString(value, "errorRecoveryMode");
  return VideoStream{stream.value(),
                     max_frame_rate,
                     ValueOrDefault(max_bit_rate, 4 << 20),
                     ValueOrDefault(protection),
                     ValueOrDefault(profile),
                     ValueOrDefault(level),
                     resolutions.value(),
                     ValueOrDefault(error_recovery_mode)};
}

absl::string_view ToString(Stream::Type type) {
  switch (type) {
    case Stream::Type::kAudioSource:
      return kAudioSourceType;
    case Stream::Type::kVideoSource:
      return kVideoSourceType;
    default: {
      OSP_NOTREACHED();
      return "";
    }
  }
}

}  // namespace

constexpr char kCastMirroring[] = "mirroring";
constexpr char kCastRemoting[] = "remoting";

// static
CastMode CastMode::Parse(absl::string_view value) {
  return (value == kCastRemoting) ? CastMode{CastMode::Type::kRemoting}
                                  : CastMode{CastMode::Type::kMirroring};
}

ErrorOr<Json::Value> Stream::ToJson() const {
  if (channels < 1 || index < 0 || codec_name.empty() ||
      target_delay.count() <= 0 ||
      target_delay.count() > std::numeric_limits<int>::max() ||
      rtp_timebase < 1) {
    return CreateParameterError("Stream");
  }

  Json::Value root;
  root["index"] = index;
  root["type"] = std::string(ToString(type));
  root["channels"] = channels;
  root["codecName"] = codec_name;
  root["rtpPayloadType"] = static_cast<int>(rtp_payload_type);
  // rtpProfile is technically required by the spec, although it is always set
  // to cast. We set it here to be compliant with all spec implementers.
  root["rtpProfile"] = "cast";
  static_assert(sizeof(ssrc) <= sizeof(Json::UInt),
                "this code assumes Ssrc fits in a Json::UInt");
  root["ssrc"] = static_cast<Json::UInt>(ssrc);
  root["targetDelay"] = static_cast<int>(target_delay.count());
  root["aesKey"] = HexEncode(aes_key);
  root["aesIvMask"] = HexEncode(aes_iv_mask);
  root["ReceiverRtcpEventLog"] = receiver_rtcp_event_log;
  root["receiverRtcpDscp"] = receiver_rtcp_dscp;
  root["timeBase"] = "1/" + std::to_string(rtp_timebase);
  return root;
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

ErrorOr<Json::Value> AudioStream::ToJson() const {
  // A bit rate of 0 is valid for some codec types, so we don't enforce here.
  if (bit_rate < 0) {
    return CreateParameterError("AudioStream");
  }

  auto error_or_stream = stream.ToJson();
  if (error_or_stream.is_error()) {
    return error_or_stream;
  }

  error_or_stream.value()["bitRate"] = bit_rate;
  return error_or_stream;
}

ErrorOr<Json::Value> Resolution::ToJson() const {
  if (width <= 0 || height <= 0) {
    return CreateParameterError("Resolution");
  }

  Json::Value root;
  root["width"] = width;
  root["height"] = height;
  return root;
}

ErrorOr<Json::Value> VideoStream::ToJson() const {
  if (max_bit_rate <= 0 || !max_frame_rate.is_positive()) {
    return CreateParameterError("VideoStream");
  }

  auto error_or_stream = stream.ToJson();
  if (error_or_stream.is_error()) {
    return error_or_stream;
  }

  auto& stream = error_or_stream.value();
  stream["maxFrameRate"] = max_frame_rate.ToString();
  stream["maxBitRate"] = max_bit_rate;
  stream["protection"] = protection;
  stream["profile"] = profile;
  stream["level"] = level;
  stream["errorRecoveryMode"] = error_recovery_mode;

  Json::Value rs;
  for (auto resolution : resolutions) {
    auto eoj = resolution.ToJson();
    if (eoj.is_error()) {
      return eoj;
    }
    rs.append(eoj.value());
  }
  stream["resolutions"] = std::move(rs);
  return error_or_stream;
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

ErrorOr<Json::Value> Offer::ToJson() const {
  Json::Value root;
  root["castMode"] = cast_mode.ToString();
  root["receiverGetStatus"] = supports_wifi_status_reporting;

  Json::Value streams;
  for (auto& as : audio_streams) {
    auto eoj = as.ToJson();
    if (eoj.is_error()) {
      return eoj;
    }
    streams.append(eoj.value());
  }

  for (auto& vs : video_streams) {
    auto eoj = vs.ToJson();
    if (eoj.is_error()) {
      return eoj;
    }
    streams.append(eoj.value());
  }

  root[kSupportedStreams] = std::move(streams);
  return root;
}

}  // namespace cast
}  // namespace openscreen
