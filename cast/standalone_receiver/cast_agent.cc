// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/cast_agent.h"

#include <utility>

#include "cast/standalone_receiver/simple_message_port.h"
#include "cast/streaming/constants.h"
#include "cast/streaming/offer_messages.h"
#include "util/json/json_serialization.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {
namespace {

////////////////////////////////////////////////////////////////////////////////
// Receiver Configuration
//
// The values defined here are constants that correspond to the Sender Demo app.
// In a production environment, these should ABSOLUTELY NOT be fixed! Instead a
// sender↔receiver OFFER/ANSWER exchange should establish them.

// In a production environment, this would start-out at some initial value
// appropriate to the networking environment, and then be adjusted by the
// application as: 1) the TYPE of the content changes (interactive, low-latency
// versus smooth, higher-latency buffered video watching); and 2) the networking
// environment reliability changes.
//
// TODO(jophba): delete these fixed values once the CastAgent handles OFFER/
// ANSWER negotiation.
constexpr std::chrono::milliseconds kTargetPlayoutDelay =
    kDefaultTargetPlayoutDelay;

const Offer kDemoOffer{
    /* .cast_mode = */ CastMode{},
    /* .supports_wifi_status_reporting = */ false,
    {AudioStream{Stream{/* .index = */ 0,
                        /* .type = */ Stream::Type::kAudioSource,
                        /* .channels = */ 2,
                        /* .codec_name = */ "opus",
                        /* .rtp_payload_type = */ RtpPayloadType::kAudioOpus,
                        /* .ssrc = */ 1,
                        /* .target_delay */ kTargetPlayoutDelay,
                        /* .aes_key = */
                        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                         0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f},
                        /* .aes_iv_mask = */
                        {0xf0, 0xe0, 0xd0, 0xc0, 0xb0, 0xa0, 0x90, 0x80, 0x70,
                         0x60, 0x50, 0x40, 0x30, 0x20, 0x10, 0x00},
                        /* .receiver_rtcp_event_log = */ false,
                        /* .receiver_rtcp_dscp = */ "",
                        // In a production environment, this would be set to the
                        // sampling rate of the audio capture.
                        /* .rtp_timebase = */ 48000},
                 /* .bit_rate = */ 0}},
    {VideoStream{
        Stream{/* .index = */ 1,
               /* .type = */ Stream::Type::kVideoSource,
               /* .channels = */ 1,
               /* .codec_name = */ "vp8",
               /* .rtp_payload_type = */ RtpPayloadType::kVideoVp8,
               /* .ssrc = */ 50001,
               /* .target_delay */ kTargetPlayoutDelay,
               /* .aes_key = */
               {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
                0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f},
               /* .aes_iv_mask = */
               {0xf1, 0xe1, 0xd1, 0xc1, 0xb1, 0xa1, 0x91, 0x81, 0x71, 0x61,
                0x51, 0x41, 0x31, 0x21, 0x11, 0x01},
               /* .receiver_rtcp_event_log = */ false,
               /* .receiver_rtcp_dscp = */ "",
               // In a production environment, this would be set to the sampling
               // rate of the audio capture.
               /* .rtp_timebase = */ static_cast<int>(kVideoTimebase::den)},
        /* .max_frame_rate = */ SimpleFraction{60, 1},
        // Approximate highend bitrate for 1080p 60fps
        /* .max_bit_rate = */ 6960000,
        /* .protection = */ "",
        /* .profile = */ "",
        /* .level = */ "",
        /* .resolutions = */ {},
        /* .error_recovery_mode = */ ""}}};

// End of Receiver Configuration.
////////////////////////////////////////////////////////////////////////////////

}  // namespace

CastAgent::CastAgent(TaskRunnerImpl* task_runner, InterfaceInfo interface)
    : task_runner_(task_runner), interface_(std::move(interface)) {
  // Create the Environment that holds the required injected dependencies
  // (clock, task runner) used throughout the system, and owns the UDP socket
  // over which all communication occurs with the Sender.
  IPEndpoint receive_endpoint{IPAddress::kV4LoopbackAddress,
                              openscreen::cast::kDefaultCastStreamingPort};
  if (interface_.GetIpAddressV4()) {
    receive_endpoint.address = interface_.GetIpAddressV4();
  } else if (interface_.GetIpAddressV6()) {
    receive_endpoint.address = interface_.GetIpAddressV6();
  }
  environment_ = std::make_unique<Environment>(&Clock::now, task_runner_,
                                               receive_endpoint);
}

CastAgent::~CastAgent() = default;

Error CastAgent::Start() {
  OSP_DCHECK(!current_session_);

  task_runner_->PostTask(
      [this] { this->wake_lock_ = ScopedWakeLock::Create(); });

  // TODO(jophba): replace start logic with binding to TLS server socket and
  // instantiating a session from the actual sender offer message.
  ReceiverSession::Preferences preferences{
      {ReceiverSession::VideoCodec::kVp8},
      {ReceiverSession::AudioCodec::kOpus}};

  auto port = std::make_unique<SimpleMessagePort>();
  auto* raw_port = port.get();

  controller_ = std::make_unique<StreamingPlaybackController>(task_runner_);
  current_session_ = std::make_unique<ReceiverSession>(
      controller_.get(), environment_.get(), std::move(port),
      std::move(preferences));

  OSP_LOG_INFO << "Injecting fake offer message...";
  auto offer_json = kDemoOffer.ToJson();
  OSP_DCHECK(offer_json.is_value());
  Json::Value message;
  message["seqNum"] = 0;
  message["type"] = "OFFER";
  message["offer"] = offer_json.value();
  auto offer_message = json::Stringify(message);
  OSP_DCHECK(offer_message.is_value());
  raw_port->ReceiveMessage(offer_message.value());

  OSP_LOG_INFO << "Awaiting first Cast Streaming packet at "
               << environment_->GetBoundLocalEndpoint() << "...";

  return Error::None();
}

Error CastAgent::Stop() {
  controller_.reset();
  current_session_.reset();
  return Error::None();
}

void CastAgent::OnError(CastSocket* socket, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket error: " << error;
}

void CastAgent::OnMessage(CastSocket* socket,
                          ::cast::channel::CastMessage message) {
  // TODO(jophba): implement message handling.
  OSP_UNIMPLEMENTED();
}

}  // namespace cast
}  // namespace openscreen
