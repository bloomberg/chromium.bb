// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

#include <chrono>  // NOLINT
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "cast/streaming/environment.h"
#include "cast/streaming/message_port.h"
#include "cast/streaming/message_util.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/receiver.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {

// JSON message field values specific to the Receiver Session.
static constexpr char kMessageTypeOffer[] = "OFFER";

// List of OFFER message fields.
static constexpr char kOfferMessageBody[] = "offer";
static constexpr char kKeyType[] = "type";
static constexpr char kSequenceNumber[] = "seqNum";

// Using statements for constructor readability.
using Preferences = ReceiverSession::Preferences;
using ConfiguredReceivers = ReceiverSession::ConfiguredReceivers;

namespace {

std::string GetCodecName(ReceiverSession::AudioCodec codec) {
  switch (codec) {
    case ReceiverSession::AudioCodec::kAac:
      return "aac";
    case ReceiverSession::AudioCodec::kOpus:
      return "opus";
  }

  OSP_NOTREACHED() << "Codec not accounted for in switch statement.";
  return {};
}

std::string GetCodecName(ReceiverSession::VideoCodec codec) {
  switch (codec) {
    case ReceiverSession::VideoCodec::kH264:
      return "h264";
    case ReceiverSession::VideoCodec::kVp8:
      return "vp8";
    case ReceiverSession::VideoCodec::kHevc:
      return "hevc";
    case ReceiverSession::VideoCodec::kVp9:
      return "vp9";
  }

  OSP_NOTREACHED() << "Codec not accounted for in switch statement.";
  return {};
}

template <typename Stream, typename Codec>
const Stream* SelectStream(const std::vector<Codec>& preferred_codecs,
                           const std::vector<Stream>& offered_streams) {
  for (Codec codec : preferred_codecs) {
    const std::string codec_name = GetCodecName(codec);
    for (const Stream& offered_stream : offered_streams) {
      if (offered_stream.stream.codec_name == codec_name) {
        OSP_VLOG << "Selected " << codec_name << " as codec for streaming.";
        return &offered_stream;
      }
    }
  }
  return nullptr;
}

}  // namespace

Preferences::Preferences() = default;
Preferences::Preferences(std::vector<VideoCodec> video_codecs,
                         std::vector<AudioCodec> audio_codecs)
    : Preferences(video_codecs, audio_codecs, nullptr, nullptr) {}

Preferences::Preferences(std::vector<VideoCodec> video_codecs,
                         std::vector<AudioCodec> audio_codecs,
                         std::unique_ptr<Constraints> constraints,
                         std::unique_ptr<DisplayDescription> description)
    : video_codecs(std::move(video_codecs)),
      audio_codecs(std::move(audio_codecs)),
      constraints(std::move(constraints)),
      display_description(std::move(description)) {}

Preferences::Preferences(Preferences&&) noexcept = default;
Preferences& Preferences::operator=(Preferences&&) noexcept = default;

ReceiverSession::ReceiverSession(Client* const client,
                                 Environment* environment,
                                 MessagePort* message_port,
                                 Preferences preferences)
    : client_(client),
      environment_(environment),
      message_port_(message_port),
      preferences_(std::move(preferences)),
      packet_router_(environment_) {
  OSP_DCHECK(client_);
  OSP_DCHECK(message_port_);
  OSP_DCHECK(environment_);

  message_port_->SetClient(this);
}

ReceiverSession::~ReceiverSession() {
  ResetReceivers();
  message_port_->SetClient(nullptr);
}

void ReceiverSession::OnMessage(absl::string_view sender_id,
                                absl::string_view message_namespace,
                                absl::string_view message) {
  ErrorOr<Json::Value> message_json = json::Parse(message);

  if (!message_json) {
    client_->OnError(this, Error::Code::kJsonParseError);
    OSP_LOG_WARN << "Received an invalid message: " << message;
    return;
  }

  // TODO(jophba): add sender connected/disconnected messaging.
  auto sequence_number = ParseInt(message_json.value(), kSequenceNumber);
  if (!sequence_number) {
    OSP_LOG_WARN << "Invalid message sequence number";
    return;
  }

  auto key_or_error = ParseString(message_json.value(), kKeyType);
  if (!key_or_error) {
    OSP_LOG_WARN << "Invalid message key";
    return;
  }

  Message parsed_message{sender_id.data(), message_namespace.data(),
                         sequence_number.value()};
  if (key_or_error.value() == kMessageTypeOffer) {
    parsed_message.body = std::move(message_json.value()[kOfferMessageBody]);
    if (parsed_message.body.isNull()) {
      OSP_LOG_WARN << "Invalid message offer body";
      return;
    }
    OnOffer(&parsed_message);
  }
}

void ReceiverSession::OnError(Error error) {
  OSP_LOG_WARN << "ReceiverSession's MessagePump encountered an error:"
               << error;
}

void ReceiverSession::OnOffer(Message* message) {
  ErrorOr<Offer> offer = Offer::Parse(std::move(message->body));
  if (!offer) {
    client_->OnError(this, offer.error());
    OSP_LOG_WARN << "Could not parse offer" << offer.error();
    return;
  }

  const AudioStream* selected_audio_stream = nullptr;
  if (!offer.value().audio_streams.empty() &&
      !preferences_.audio_codecs.empty()) {
    selected_audio_stream =
        SelectStream(preferences_.audio_codecs, offer.value().audio_streams);
  }

  const VideoStream* selected_video_stream = nullptr;
  if (!offer.value().video_streams.empty() &&
      !preferences_.video_codecs.empty()) {
    selected_video_stream =
        SelectStream(preferences_.video_codecs, offer.value().video_streams);
  }

  cast_mode_ = offer.value().cast_mode;
  auto receivers =
      TrySpawningReceivers(selected_audio_stream, selected_video_stream);
  if (receivers) {
    const Answer answer =
        ConstructAnswer(message, selected_audio_stream, selected_video_stream);
    client_->OnNegotiated(this, std::move(receivers.value()));

    message->body = answer.ToAnswerMessage();
  } else {
    message->body = CreateInvalidAnswer(receivers.error());
  }

  SendMessage(message);
}

std::pair<SessionConfig, std::unique_ptr<Receiver>>
ReceiverSession::ConstructReceiver(const Stream& stream) {
  SessionConfig config = {stream.ssrc,         stream.ssrc + 1,
                          stream.rtp_timebase, stream.channels,
                          stream.target_delay, stream.aes_key,
                          stream.aes_iv_mask};
  auto receiver =
      std::make_unique<Receiver>(environment_, &packet_router_, config);

  return std::make_pair(std::move(config), std::move(receiver));
}

ErrorOr<ConfiguredReceivers> ReceiverSession::TrySpawningReceivers(
    const AudioStream* audio,
    const VideoStream* video) {
  if (!audio && !video) {
    return Error::Code::kParameterInvalid;
  }

  ResetReceivers();

  absl::optional<ConfiguredReceiver<AudioStream>> audio_receiver;
  absl::optional<ConfiguredReceiver<VideoStream>> video_receiver;

  if (audio) {
    auto audio_pair = ConstructReceiver(audio->stream);
    current_audio_receiver_ = std::move(audio_pair.second);
    audio_receiver.emplace(ConfiguredReceiver<AudioStream>{
        current_audio_receiver_.get(), std::move(audio_pair.first), *audio});
  }

  if (video) {
    auto video_pair = ConstructReceiver(video->stream);
    current_video_receiver_ = std::move(video_pair.second);
    video_receiver.emplace(ConfiguredReceiver<VideoStream>{
        current_video_receiver_.get(), std::move(video_pair.first), *video});
  }

  return ConfiguredReceivers{std::move(audio_receiver),
                             std::move(video_receiver)};
}

void ReceiverSession::ResetReceivers() {
  if (current_video_receiver_ || current_audio_receiver_) {
    client_->OnConfiguredReceiversDestroyed(this);
    current_audio_receiver_.reset();
    current_video_receiver_.reset();
  }
}

Answer ReceiverSession::ConstructAnswer(
    Message* message,
    const AudioStream* selected_audio_stream,
    const VideoStream* selected_video_stream) {
  OSP_DCHECK(selected_audio_stream || selected_video_stream);

  std::vector<int> stream_indexes;
  std::vector<Ssrc> stream_ssrcs;
  if (selected_audio_stream) {
    stream_indexes.push_back(selected_audio_stream->stream.index);
    stream_ssrcs.push_back(selected_audio_stream->stream.ssrc + 1);
  }

  if (selected_video_stream) {
    stream_indexes.push_back(selected_video_stream->stream.index);
    stream_ssrcs.push_back(selected_video_stream->stream.ssrc + 1);
  }

  absl::optional<Constraints> constraints;
  if (preferences_.constraints) {
    constraints = *preferences_.constraints;
  }

  absl::optional<DisplayDescription> display;
  if (preferences_.display_description) {
    display = *preferences_.display_description;
  }

  return Answer{cast_mode_,
                environment_->GetBoundLocalEndpoint().port,
                std::move(stream_indexes),
                std::move(stream_ssrcs),
                constraints,
                display,
                std::vector<int>{},  // receiver_rtcp_event_log
                std::vector<int>{},  // receiver_rtcp_dscp
                supports_wifi_status_reporting_};
}

void ReceiverSession::SendMessage(Message* message) {
  // All messages have the sequence number embedded.
  message->body[kSequenceNumber] = message->sequence_number;

  auto body_or_error = json::Stringify(message->body);
  if (body_or_error.is_value()) {
    message_port_->PostMessage(message->sender_id, message->message_namespace,
                               body_or_error.value());
  } else {
    client_->OnError(this, body_or_error.error());
  }
}

}  // namespace cast
}  // namespace openscreen
