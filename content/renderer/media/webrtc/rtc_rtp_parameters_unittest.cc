// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_parameters.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(RTCRtpParametersTest, Read) {
  webrtc::RtpParameters webrtc_parameters;
  webrtc_parameters.transaction_id = "transaction_id";
  webrtc_parameters.degradation_preference =
      webrtc::DegradationPreference::BALANCED;

  webrtc_parameters.encodings.emplace_back();
  webrtc::RtpEncodingParameters& webrtc_encoding =
      webrtc_parameters.encodings.front();
  webrtc_encoding.codec_payload_type = 42;
  webrtc_encoding.dtx = webrtc::DtxStatus::ENABLED;
  webrtc_encoding.active = true;
  webrtc_encoding.bitrate_priority = webrtc::kDefaultBitratePriority;
  webrtc_encoding.ptime = 1337;
  webrtc_encoding.max_bitrate_bps = 1337000;
  webrtc_encoding.max_framerate = 60;
  webrtc_encoding.scale_resolution_down_by = 1.;
  webrtc_encoding.rid = "rid";

  webrtc_parameters.header_extensions.emplace_back();
  webrtc::RtpHeaderExtensionParameters& webrtc_header_extension =
      webrtc_parameters.header_extensions.front();
  webrtc_header_extension.uri = "uri";
  webrtc_header_extension.id = 33;
  webrtc_header_extension.encrypt = true;

  webrtc_parameters.codecs.emplace_back();
  webrtc::RtpCodecParameters& webrtc_codec_parameter =
      webrtc_parameters.codecs.front();
  webrtc_codec_parameter.payload_type = 42;
  webrtc_codec_parameter.clock_rate = 1234;
  webrtc_codec_parameter.num_channels = 2;

  blink::WebRTCRtpParameters parameters =
      GetWebRTCRtpParameters(webrtc_parameters);
  EXPECT_EQ(parameters.TransactionId(), "transaction_id");
  EXPECT_EQ(parameters.DegradationPreference(),
            blink::WebRTCDegradationPreference::Balanced);

  ASSERT_EQ(parameters.Encodings().size(), 1u);
  for (const auto& encoding : parameters.Encodings()) {
    EXPECT_EQ(encoding.CodecPayloadType(), 42);
    EXPECT_EQ(encoding.Dtx(), blink::WebRTCDtxStatus::Enabled);
    EXPECT_EQ(encoding.Active(), true);
    EXPECT_EQ(encoding.Priority(), blink::WebRTCPriorityType::Low);
    EXPECT_EQ(encoding.Ptime(), 1337u);
    EXPECT_EQ(encoding.MaxBitrate(), 1337000u);
    EXPECT_EQ(encoding.MaxFramerate(), 60u);
    EXPECT_EQ(encoding.ScaleResolutionDownBy(), 1.);
    EXPECT_EQ(encoding.Rid(), "rid");
  }

  ASSERT_EQ(parameters.HeaderExtensions().size(), 1u);
  for (const auto& header_extension : parameters.HeaderExtensions()) {
    EXPECT_EQ(header_extension.URI(), "uri");
    EXPECT_EQ(header_extension.Id(), 33);
    EXPECT_EQ(header_extension.Encrypted(), true);
  }

  ASSERT_EQ(parameters.Codecs().size(), 1u);
  for (const auto& codec : parameters.Codecs()) {
    EXPECT_EQ(codec.PayloadType(), 42);
    EXPECT_EQ(codec.ClockRate(), 1234u);
    EXPECT_EQ(codec.Channels(), 2u);
  }
}

TEST(RTCRtpParametersTest, WriteEncodingParameters) {
  blink::WebRTCRtpEncodingParameters web_encoding_parameters(
      42, blink::WebRTCDtxStatus::Enabled, true,
      blink::WebRTCPriorityType::High, 1337, 50000, 60, 0.5, "rid");
  webrtc::RtpEncodingParameters encoding_parameters =
      content::FromWebRTCRtpEncodingParameters(web_encoding_parameters);

  EXPECT_EQ(encoding_parameters.codec_payload_type, 42);
  EXPECT_EQ(encoding_parameters.dtx, webrtc::DtxStatus::ENABLED);
  EXPECT_EQ(encoding_parameters.active, true);
  EXPECT_EQ(encoding_parameters.bitrate_priority, 4.0);
  EXPECT_EQ(encoding_parameters.ptime, 1337);
  EXPECT_EQ(encoding_parameters.max_bitrate_bps, 50000);
  EXPECT_EQ(encoding_parameters.max_framerate, 60);
  EXPECT_EQ(encoding_parameters.scale_resolution_down_by, 0.5);
  EXPECT_EQ(encoding_parameters.rid, "rid");
}

TEST(RTCRtpParametersTest, CheckDtxStatusEnum) {
  webrtc::RtpEncodingParameters webrtc_encoding_parameters;

  {
    webrtc_encoding_parameters.dtx = webrtc::DtxStatus::DISABLED;
    blink::WebRTCRtpEncodingParameters encoding_parameters =
        GetWebRTCRtpEncodingParameters(webrtc_encoding_parameters);
    EXPECT_EQ(encoding_parameters.Dtx(), blink::WebRTCDtxStatus::Disabled);
  }

  {
    webrtc_encoding_parameters.dtx = webrtc::DtxStatus::ENABLED;
    blink::WebRTCRtpEncodingParameters encoding_parameters =
        GetWebRTCRtpEncodingParameters(webrtc_encoding_parameters);
    EXPECT_EQ(encoding_parameters.Dtx(), blink::WebRTCDtxStatus::Enabled);
  }
}

TEST(RTCRtpParametersTest, CheckDegradationPreferenceEnum) {
  webrtc::RtpParameters webrtc_parameters;

  {
    webrtc_parameters.degradation_preference =
        webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
    blink::WebRTCRtpParameters parameters =
        GetWebRTCRtpParameters(webrtc_parameters);
    EXPECT_EQ(parameters.DegradationPreference(),
              blink::WebRTCDegradationPreference::MaintainFramerate);
  }

  {
    webrtc_parameters.degradation_preference =
        webrtc::DegradationPreference::MAINTAIN_RESOLUTION;
    blink::WebRTCRtpParameters parameters =
        GetWebRTCRtpParameters(webrtc_parameters);
    EXPECT_EQ(parameters.DegradationPreference(),
              blink::WebRTCDegradationPreference::MaintainResolution);
  }

  {
    webrtc_parameters.degradation_preference =
        webrtc::DegradationPreference::BALANCED;
    blink::WebRTCRtpParameters parameters =
        GetWebRTCRtpParameters(webrtc_parameters);
    EXPECT_EQ(parameters.DegradationPreference(),
              blink::WebRTCDegradationPreference::Balanced);
  }
}

TEST(RTCRtpParametersTest, CheckPriorityEnum) {
  webrtc::RtpEncodingParameters webrtc_encoding_parameters;

  {
    webrtc_encoding_parameters.bitrate_priority =
        webrtc::kDefaultBitratePriority / 2;
    blink::WebRTCRtpEncodingParameters encoding_parameters =
        GetWebRTCRtpEncodingParameters(webrtc_encoding_parameters);
    EXPECT_EQ(encoding_parameters.Priority(),
              blink::WebRTCPriorityType::VeryLow);
  }

  {
    webrtc_encoding_parameters.bitrate_priority =
        webrtc::kDefaultBitratePriority;
    blink::WebRTCRtpEncodingParameters encoding_parameters =
        GetWebRTCRtpEncodingParameters(webrtc_encoding_parameters);
    EXPECT_EQ(encoding_parameters.Priority(), blink::WebRTCPriorityType::Low);
  }

  {
    webrtc_encoding_parameters.bitrate_priority =
        webrtc::kDefaultBitratePriority * 2;
    blink::WebRTCRtpEncodingParameters encoding_parameters =
        GetWebRTCRtpEncodingParameters(webrtc_encoding_parameters);
    EXPECT_EQ(encoding_parameters.Priority(),
              blink::WebRTCPriorityType::Medium);
  }

  {
    webrtc_encoding_parameters.bitrate_priority =
        webrtc::kDefaultBitratePriority * 4;
    blink::WebRTCRtpEncodingParameters encoding_parameters =
        GetWebRTCRtpEncodingParameters(webrtc_encoding_parameters);
    EXPECT_EQ(encoding_parameters.Priority(), blink::WebRTCPriorityType::High);
  }
}

}  // namespace content
