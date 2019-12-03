// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/answer_messages.h"

#include <iomanip>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "platform/base/error.h"
#include "util/logging.h"

namespace cast {
namespace streaming {

namespace {

Json::Value ScalingToJson(Scaling scaling) {
  switch (scaling) {
    case Scaling::kReceiver:
      return Json::Value("receiver");
    case Scaling::kSender:
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

openscreen::Error CreateParameterSerializationError(absl::string_view type) {
  return openscreen::Error(
      openscreen::Error::Code::kParameterInvalid,
      absl::StrCat("Invalid '", type, "' presented for serialization."));
}
}  // namespace

openscreen::ErrorOr<Json::Value> AudioConstraints::ToJson() const {
  if (max_sample_rate <= 0 || max_channels <= 0 || min_bit_rate <= 0 ||
      max_bit_rate < min_bit_rate) {
    return CreateParameterSerializationError("AudioConstraints");
  }

  Json::Value root;
  root["maxSampleRate"] = max_sample_rate;
  root["maxChannels"] = max_channels;
  root["minBitRate"] = min_bit_rate;
  root["maxBitRate"] = max_bit_rate;
  root["maxDelay"] = max_delay.count();
  return root;
}

openscreen::ErrorOr<Json::Value> Dimensions::ToJson() const {
  if (width <= 0 || height <= 0 || frame_rate_denominator <= 0 ||
      frame_rate_numerator <= 0) {
    return CreateParameterSerializationError("Dimensions");
  }

  Json::Value root;
  root["width"] = width;
  root["height"] = height;

  if (frame_rate_denominator > 1) {
    root["frameRate"] =
        absl::StrCat(frame_rate_numerator, "/", frame_rate_denominator);
  } else {
    root["frameRate"] = std::to_string(frame_rate_numerator);
  }
  return root;
}

openscreen::ErrorOr<Json::Value> VideoConstraints::ToJson() const {
  if (max_pixels_per_second <= 0 || min_bit_rate <= 0 ||
      max_bit_rate < min_bit_rate || max_delay.count() <= 0) {
    return CreateParameterSerializationError("VideoConstraints");
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
  root["maxDelay"] = max_delay.count();
  return root;
}

openscreen::ErrorOr<Json::Value> Constraints::ToJson() const {
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

openscreen::ErrorOr<Json::Value> DisplayDescription::ToJson() const {
  if (aspect_ratio.empty()) {
    return CreateParameterSerializationError("DisplayDescription");
  }

  auto dimensions_or_error = dimensions.ToJson();
  if (dimensions_or_error.is_error()) {
    return dimensions_or_error.error();
  }

  Json::Value root;
  root["dimensions"] = dimensions_or_error.value();
  root["aspectRatio"] = aspect_ratio;
  root["scaling"] = ScalingToJson(scaling);
  return root;
}

openscreen::ErrorOr<Json::Value> Answer::ToJson() const {
  if (udp_port <= 0 || udp_port > 65535) {
    return CreateParameterSerializationError("Answer - UDP Port number");
  }

  auto constraints_or_error = constraints.ToJson();
  if (constraints_or_error.is_error()) {
    return constraints_or_error.error();
  }

  auto display_or_error = display.ToJson();
  if (display_or_error.is_error()) {
    return display_or_error.error();
  }

  Json::Value root;
  root["udpPort"] = udp_port;
  root["sendIndexes"] = PrimitiveVectorToJson(send_indexes);
  root["ssrcs"] = PrimitiveVectorToJson(ssrcs);
  root["constraints"] = constraints_or_error.value();
  root["display"] = display_or_error.value();
  root["receiverRtcpEventLog"] = PrimitiveVectorToJson(receiver_rtcp_event_log);
  root["receiverRtcpDscp"] = PrimitiveVectorToJson(receiver_rtcp_dscp);
  root["receiverGetStatus"] = supports_wifi_status_reporting;
  root["rtpExtensions"] = PrimitiveVectorToJson(rtp_extensions);
  return root;
}

}  // namespace streaming
}  // namespace cast
