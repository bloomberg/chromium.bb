// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/answer_messages.h"

#include <utility>

#include "absl/strings/str_cat.h"
#include "cast/streaming/message_util.h"
#include "platform/base/error.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {

namespace {

static constexpr char kMessageKeyType[] = "type";
static constexpr char kMessageTypeAnswer[] = "ANSWER";

// List of ANSWER message fields.
static constexpr char kAnswerMessageBody[] = "answer";
static constexpr char kResult[] = "result";
static constexpr char kResultOk[] = "ok";
static constexpr char kResultError[] = "error";
static constexpr char kErrorMessageBody[] = "error";
static constexpr char kErrorCode[] = "code";
static constexpr char kErrorDescription[] = "description";

Json::Value AspectRatioConstraintToJson(AspectRatioConstraint aspect_ratio) {
  switch (aspect_ratio) {
    case AspectRatioConstraint::kVariable:
      return Json::Value("receiver");
    case AspectRatioConstraint::kFixed:
    default:
      return Json::Value("sender");
  }
}

template <typename T>
Json::Value PrimitiveVectorToJson(const std::vector<T>& vec) {
  Json::Value array(Json::ValueType::arrayValue);
  array.resize(vec.size());

  for (Json::Value::ArrayIndex i = 0; i < vec.size(); ++i) {
    array[i] = Json::Value(vec[i]);
  }

  return array;
}

}  // namespace

ErrorOr<Json::Value> AudioConstraints::ToJson() const {
  if (max_sample_rate <= 0 || max_channels <= 0 || min_bit_rate <= 0 ||
      max_bit_rate < min_bit_rate) {
    return CreateParameterError("AudioConstraints");
  }

  Json::Value root;
  root["maxSampleRate"] = max_sample_rate;
  root["maxChannels"] = max_channels;
  root["minBitRate"] = min_bit_rate;
  root["maxBitRate"] = max_bit_rate;
  root["maxDelay"] = Json::Value::Int64(max_delay.count());
  return root;
}

ErrorOr<Json::Value> Dimensions::ToJson() const {
  if (width <= 0 || height <= 0 || !frame_rate.is_defined() ||
      !frame_rate.is_positive()) {
    return CreateParameterError("Dimensions");
  }

  Json::Value root;
  root["width"] = width;
  root["height"] = height;
  root["frameRate"] = frame_rate.ToString();
  return root;
}

ErrorOr<Json::Value> VideoConstraints::ToJson() const {
  if (max_pixels_per_second <= 0 || min_bit_rate <= 0 ||
      max_bit_rate < min_bit_rate || max_delay.count() <= 0) {
    return CreateParameterError("VideoConstraints");
  }

  auto error_or_min_dim = min_dimensions.ToJson();
  if (error_or_min_dim.is_error()) {
    return error_or_min_dim.error();
  }

  auto error_or_max_dim = max_dimensions.ToJson();
  if (error_or_max_dim.is_error()) {
    return error_or_max_dim.error();
  }

  Json::Value root;
  root["maxPixelsPerSecond"] = max_pixels_per_second;
  root["minDimensions"] = error_or_min_dim.value();
  root["maxDimensions"] = error_or_max_dim.value();
  root["minBitRate"] = min_bit_rate;
  root["maxBitRate"] = max_bit_rate;
  root["maxDelay"] = Json::Value::Int64(max_delay.count());
  return root;
}

ErrorOr<Json::Value> Constraints::ToJson() const {
  auto audio_or_error = audio.ToJson();
  if (audio_or_error.is_error()) {
    return audio_or_error.error();
  }

  auto video_or_error = video.ToJson();
  if (video_or_error.is_error()) {
    return video_or_error.error();
  }

  Json::Value root;
  root["audio"] = audio_or_error.value();
  root["video"] = video_or_error.value();
  return root;
}

ErrorOr<Json::Value> DisplayDescription::ToJson() const {
  if (aspect_ratio.width < 1 || aspect_ratio.height < 1) {
    return CreateParameterError("DisplayDescription");
  }

  auto dimensions_or_error = dimensions.ToJson();
  if (dimensions_or_error.is_error()) {
    return dimensions_or_error.error();
  }

  Json::Value root;
  root["dimensions"] = dimensions_or_error.value();
  root["aspectRatio"] =
      absl::StrCat(aspect_ratio.width, ":", aspect_ratio.height);
  root["scaling"] = AspectRatioConstraintToJson(aspect_ratio_constraint);
  return root;
}

ErrorOr<Json::Value> Answer::ToJson() const {
  if (udp_port <= 0 || udp_port > 65535) {
    return CreateParameterError("Answer - UDP Port number");
  }

  Json::Value root;
  if (constraints) {
    auto constraints_or_error = constraints.value().ToJson();
    if (constraints_or_error.is_error()) {
      return constraints_or_error.error();
    }
    root["constraints"] = constraints_or_error.value();
  }

  if (display) {
    auto display_or_error = display.value().ToJson();
    if (display_or_error.is_error()) {
      return display_or_error.error();
    }
    root["display"] = display_or_error.value();
  }

  root["castMode"] = cast_mode.ToString();
  root["udpPort"] = udp_port;
  root["receiverGetStatus"] = supports_wifi_status_reporting;
  root["sendIndexes"] = PrimitiveVectorToJson(send_indexes);
  root["ssrcs"] = PrimitiveVectorToJson(ssrcs);
  if (!receiver_rtcp_event_log.empty()) {
    root["receiverRtcpEventLog"] =
        PrimitiveVectorToJson(receiver_rtcp_event_log);
  }
  if (!receiver_rtcp_dscp.empty()) {
    root["receiverRtcpDscp"] = PrimitiveVectorToJson(receiver_rtcp_dscp);
  }
  if (!rtp_extensions.empty()) {
    root["rtpExtensions"] = PrimitiveVectorToJson(rtp_extensions);
  }
  return root;
}

Json::Value Answer::ToAnswerMessage() const {
  auto json_or_error = ToJson();
  if (json_or_error.is_error()) {
    return CreateInvalidAnswer(json_or_error.error());
  }

  Json::Value message_root;
  message_root[kMessageKeyType] = kMessageTypeAnswer;
  message_root[kAnswerMessageBody] = std::move(json_or_error.value());
  message_root[kResult] = kResultOk;
  return message_root;
}

Json::Value CreateInvalidAnswer(Error error) {
  Json::Value message_root;
  message_root[kMessageKeyType] = kMessageTypeAnswer;
  message_root[kResult] = kResultError;
  message_root[kErrorMessageBody][kErrorCode] = static_cast<int>(error.code());
  message_root[kErrorMessageBody][kErrorDescription] = error.message();

  return message_root;
}

}  // namespace cast
}  // namespace openscreen
