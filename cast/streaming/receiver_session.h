// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RECEIVER_SESSION_H_
#define CAST_STREAMING_RECEIVER_SESSION_H_

#include <memory>
#include <vector>

#include "cast/streaming/environment.h"
#include "cast/streaming/message_port.h"
#include "cast/streaming/offer_messages.h"
#include "cast/streaming/receiver.h"
#include "cast/streaming/receiver_packet_router.h"
#include "cast/streaming/session_config.h"
#include "util/json/json_serialization.h"

namespace cast {

namespace channel {
class CastSocket;
class CastMessage;
class VirtualConnectionRouter;
class VirtualConnection;
}  // namespace channel

namespace streaming {

class ReceiverSession final : public MessagePort::Client {
 public:
  // Upon successful negotiation, a set of configured receivers is constructed
  // for handling audio and video. Note that either receiver may be null.
  class ConfiguredReceivers {
   public:
    // In practice, we may have 0, 1, or 2 receivers configured, depending
    // on if the device supports audio and video, and if we were able to
    // successfully negotiate a receiver configuration.
    ConfiguredReceivers(
        std::unique_ptr<Receiver> audio_receiver,
        const absl::optional<SessionConfig> audio_receiver_config,
        std::unique_ptr<Receiver> video_receiver,
        const absl::optional<SessionConfig> video_receiver_config);
    ConfiguredReceivers(const ConfiguredReceivers&) = delete;
    ConfiguredReceivers(ConfiguredReceivers&&) noexcept;
    ConfiguredReceivers& operator=(const ConfiguredReceivers&) = delete;
    ConfiguredReceivers& operator=(ConfiguredReceivers&&) noexcept;
    ~ConfiguredReceivers();

    // If the receiver is audio- or video-only, either of the receivers
    // may be nullptr. However, in the majority of cases they will be populated.
    Receiver* audio_receiver() const { return audio_receiver_.get(); }
    const absl::optional<SessionConfig>& audio_session_config() const {
      return audio_receiver_config_;
    }
    Receiver* video_receiver() const { return video_receiver_.get(); }
    const absl::optional<SessionConfig>& video_session_config() const {
      return video_receiver_config_;
    }

   private:
    std::unique_ptr<Receiver> audio_receiver_;
    absl::optional<SessionConfig> audio_receiver_config_;
    std::unique_ptr<Receiver> video_receiver_;
    absl::optional<SessionConfig> video_receiver_config_;
  };

  // The embedder should provide a client for handling connections.
  // When a connection is established, the OnNegotiated callback is called.
  class Client {
   public:
    virtual void OnNegotiated(ReceiverSession* session,
                              ConfiguredReceivers receivers) = 0;
    virtual void OnError(ReceiverSession* session, openscreen::Error error) = 0;
  };

  // The embedder has the option of providing a list of prioritized
  // preferences for selecting from the offer.
  enum class AudioCodec : int { kAac, kOpus };
  enum class VideoCodec : int { kH264, kVp8, kHevc, kVp9 };

  // Note: embedders are required to implement the following
  // codecs to be Cast V2 compliant: H264, VP8, AAC, Opus.
  // TODO(jophba): add additional fields for preferences.
  struct Preferences {
    Preferences();
    Preferences(std::vector<VideoCodec> video_codecs,
                std::vector<AudioCodec> audio_codecs);

    Preferences(Preferences&&) noexcept;
    Preferences(const Preferences&);
    Preferences& operator=(Preferences&&) noexcept;
    Preferences& operator=(const Preferences&);

    std::vector<VideoCodec> video_codecs{VideoCodec::kVp8, VideoCodec::kH264};
    std::vector<AudioCodec> audio_codecs{AudioCodec::kOpus, AudioCodec::kAac};
  };

  ReceiverSession(Client* const client,
                  std::unique_ptr<MessagePort> message_port,
                  std::unique_ptr<Environment> environment,
                  Preferences preferences);
  ReceiverSession(const ReceiverSession&) = delete;
  ReceiverSession(ReceiverSession&&) = delete;
  ReceiverSession& operator=(const ReceiverSession&) = delete;
  ReceiverSession& operator=(ReceiverSession&&) = delete;
  ~ReceiverSession();

  // MessagePort::Client overrides
  void OnMessage(absl::string_view sender_id,
                 absl::string_view namespace_,
                 absl::string_view message) override;
  void OnError(openscreen::Error error) override;

 private:
  // Message handlers
  void OnOffer(Json::Value root, int sequence_number);

  void SelectStreams(const AudioStream* audio,
                     const VideoStream* video,
                     Offer&& offer);

  Client* const client_;
  const std::unique_ptr<MessagePort> message_port_;
  const std::unique_ptr<Environment> environment_;
  const Preferences preferences_;

  ReceiverPacketRouter packet_router_;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STREAMING_RECEIVER_SESSION_H_
