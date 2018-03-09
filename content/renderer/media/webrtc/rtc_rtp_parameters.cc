// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_parameters.h"

#include <utility>

namespace {

// Relative weights for each priority as defined in RTCWEB-DATA
// https://tools.ietf.org/html/draft-ietf-rtcweb-data-channel
const double kPriorityWeightVeryLow = 0.5;
const double kPriorityWeightLow = 1;
const double kPriorityWeightMedium = 2;
const double kPriorityWeightHigh = 4;

template <typename T, typename F>
base::Optional<T> ToBaseOptional(const rtc::Optional<F>& from) {
  if (from)
    return from.value();
  return base::nullopt;
}

blink::WebRTCPriorityType PriorityFromDouble(double priority) {
  // Find the middle point between 2 priority weights to match them to a
  // WebRTC priority
  const double very_low_upper_bound =
      (kPriorityWeightVeryLow + kPriorityWeightLow) / 2;
  const double low_upper_bound =
      (kPriorityWeightLow + kPriorityWeightMedium) / 2;
  const double medium_upper_bound =
      (kPriorityWeightMedium + kPriorityWeightHigh) / 2;

  if (priority < webrtc::kDefaultBitratePriority * very_low_upper_bound) {
    return blink::WebRTCPriorityType::VeryLow;
  }
  if (priority < webrtc::kDefaultBitratePriority * low_upper_bound) {
    return blink::WebRTCPriorityType::Low;
  }
  if (priority < webrtc::kDefaultBitratePriority * medium_upper_bound) {
    return blink::WebRTCPriorityType::Medium;
  }
  return blink::WebRTCPriorityType::High;
}

base::Optional<blink::WebRTCDtxStatus> FromRTCDtxStatus(
    rtc::Optional<webrtc::DtxStatus> status) {
  if (!status)
    return base::nullopt;

  blink::WebRTCDtxStatus result;
  switch (status.value()) {
    case webrtc::DtxStatus::DISABLED:
      result = blink::WebRTCDtxStatus::Disabled;
      break;
    case webrtc::DtxStatus::ENABLED:
      result = blink::WebRTCDtxStatus::Enabled;
      break;
    default:
      NOTREACHED();
  }
  return result;
}

base::Optional<blink::WebRTCDegradationPreference> FromRTCDegradationPreference(
    rtc::Optional<webrtc::DegradationPreference> degradation_preference) {
  if (!degradation_preference)
    return base::nullopt;

  blink::WebRTCDegradationPreference result;
  switch (degradation_preference.value()) {
    case webrtc::DegradationPreference::MAINTAIN_FRAMERATE:
      result = blink::WebRTCDegradationPreference::MaintainFramerate;
      break;
    case webrtc::DegradationPreference::MAINTAIN_RESOLUTION:
      result = blink::WebRTCDegradationPreference::MaintainResolution;
      break;
    case webrtc::DegradationPreference::BALANCED:
      result = blink::WebRTCDegradationPreference::Balanced;
      break;
    default:
      NOTREACHED();
  }
  return result;
}

}  // namespace

namespace content {

blink::WebRTCRtpParameters GetWebRTCRtpParameters(
    const webrtc::RtpParameters& parameters) {
  blink::WebVector<blink::WebRTCRtpEncodingParameters> encodings;
  encodings.reserve(parameters.encodings.size());
  for (const auto& encoding_parameter : parameters.encodings) {
    encodings.emplace_back(GetWebRTCRtpEncodingParameters(encoding_parameter));
  }

  blink::WebVector<blink::WebRTCRtpHeaderExtensionParameters> header_extensions;
  header_extensions.reserve(parameters.header_extensions.size());
  for (const auto& extension_parameter : parameters.header_extensions) {
    header_extensions.emplace_back(
        GetWebRTCRtpHeaderExtensionParameters(extension_parameter));
  }

  blink::WebVector<blink::WebRTCRtpCodecParameters> codec_parameters;
  codec_parameters.reserve(parameters.codecs.size());
  for (const auto& codec_parameter : parameters.codecs) {
    codec_parameters.emplace_back(GetWebRTCRtpCodecParameters(codec_parameter));
  }

  return blink::WebRTCRtpParameters(
      blink::WebString::FromASCII(parameters.transaction_id),
      blink::WebRTCRtcpParameters(), std::move(encodings), header_extensions,
      codec_parameters,
      FromRTCDegradationPreference(parameters.degradation_preference));
}

blink::WebRTCRtpEncodingParameters GetWebRTCRtpEncodingParameters(
    const webrtc::RtpEncodingParameters& encoding_parameters) {
  return blink::WebRTCRtpEncodingParameters(
      ToBaseOptional<uint8_t>(encoding_parameters.codec_payload_type),
      FromRTCDtxStatus(encoding_parameters.dtx), encoding_parameters.active,
      PriorityFromDouble(encoding_parameters.bitrate_priority),
      ToBaseOptional<uint32_t>(encoding_parameters.ptime),
      ToBaseOptional<uint32_t>(encoding_parameters.max_bitrate_bps),
      ToBaseOptional<uint32_t>(encoding_parameters.max_framerate),
      encoding_parameters.scale_framerate_down_by,
      blink::WebString::FromASCII(encoding_parameters.rid));
}

blink::WebRTCRtpHeaderExtensionParameters GetWebRTCRtpHeaderExtensionParameters(
    const webrtc::RtpHeaderExtensionParameters& extension_parameters) {
  return blink::WebRTCRtpHeaderExtensionParameters(
      blink::WebString::FromASCII(extension_parameters.uri),
      extension_parameters.id, extension_parameters.encrypt);
}

// TODO(orphis): Copy the RTCP information
// https://crbug.com/webrtc/7580
blink::WebRTCRtcpParameters GetWebRTCRtcpParameters() {
  return blink::WebRTCRtcpParameters();
}

blink::WebRTCRtpCodecParameters GetWebRTCRtpCodecParameters(
    const webrtc::RtpCodecParameters& codec_parameters) {
  return blink::WebRTCRtpCodecParameters(
      codec_parameters.payload_type,
      blink::WebString::FromASCII(codec_parameters.mime_type()),
      ToBaseOptional<uint32_t>(codec_parameters.clock_rate),
      ToBaseOptional<uint16_t>(codec_parameters.num_channels),
      // TODO(orphis): Convert the parameters field to sdpFmtpLine
      // https://crbug.com/webrtc/7580
      blink::WebString());
}

}  // namespace content
