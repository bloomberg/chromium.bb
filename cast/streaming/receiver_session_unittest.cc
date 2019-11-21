// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/receiver_session.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

using openscreen::platform::Clock;
using openscreen::platform::FakeClock;
using openscreen::platform::FakeTaskRunner;

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;

namespace cast {
namespace streaming {

namespace {

const std::string kValidOfferMessage = R"({
  "type": "OFFER",
  "seqNum": 1337,
  "offer": {
    "castMode": "mirroring",
    "receiverGetStatus": true,
    "supportedStreams": [
      {
        "index": 31337,
        "type": "video_source",
        "codecName": "vp9",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088743,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "bbf109bf84513b456b13a184453b66ce",
        "aesIvMask": "edaf9e4536e2b66191f560d9c04b2a69",
        "resolutions": [
          {
            "width": 1280,
            "height": 720
          }
        ]
      },
      {
        "index": 31338,
        "type": "video_source",
        "codecName": "vp8",
        "rtpProfile": "cast",
        "rtpPayloadType": 127,
        "ssrc": 19088745,
        "maxFrameRate": "60000/1000",
        "timeBase": "1/90000",
        "maxBitRate": 5000000,
        "profile": "main",
        "level": "4",
        "aesKey": "040d756791711fd3adb939066e6d8690",
        "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
        "resolutions": [
          {
            "width": 1280,
            "height": 720
          }
        ]
      },
      {
        "index": 1337,
        "type": "audio_source",
        "codecName": "opus",
        "rtpProfile": "cast",
        "rtpPayloadType": 97,
        "ssrc": 19088747,
        "bitRate": 124000,
        "timeBase": "1/48000",
        "channels": 2,
        "aesKey": "51027e4e2347cbcb49d57ef10177aebc",
        "aesIvMask": "7f12a19be62a36c04ae4116caaeff6d1"
      }
    ]
  }
})";

class SimpleMessagePort : public MessagePort {
 public:
  ~SimpleMessagePort() override{};
  void SetClient(MessagePort::Client* client) { client_ = client; }

  void ReceiveMessage(absl::string_view message) {
    ASSERT_NE(client_, nullptr);
    client_->OnMessage("sender-id", "namespace", message);
  }

  void ReceiveError(openscreen::Error error) {
    ASSERT_NE(client_, nullptr);
    client_->OnError(error);
  }

  void PostMessage(absl::string_view message) {
    posted_messages_.emplace_back(std::move(message));
  }

  void Close() { is_closed_ = true; }

  MessagePort::Client* client_ = nullptr;
  bool is_closed_ = false;
  std::vector<std::string> posted_messages_;
};

class FakeClient : public ReceiverSession::Client {
 public:
  MOCK_METHOD(void,
              OnNegotiated,
              (ReceiverSession*, ReceiverSession::ConfiguredReceivers));
  MOCK_METHOD(void, OnError, (ReceiverSession*, openscreen::Error error));
};

}  // namespace

class ReceiverSessionTest : public ::testing::Test {
 public:
  ReceiverSessionTest()
      : clock_(Clock::time_point{}),
        task_runner_(&clock_),
        env_(std::make_unique<Environment>(&FakeClock::now,
                                           &task_runner_,
                                           openscreen::IPEndpoint{})) {}

  FakeClock clock_;
  FakeTaskRunner task_runner_;
  std::unique_ptr<Environment> env_;
};

TEST_F(ReceiverSessionTest, RegistersSelfOnMessagePump) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  // This should be safe, since the message_port location should not move
  // just because of being moved into the ReceiverSession.
  SimpleMessagePort* raw_port = message_port.get();
  StrictMock<FakeClient> client;

  auto session = std::make_unique<ReceiverSession>(
      &client, std::move(message_port), std::move(env_),
      ReceiverSession::Preferences{});
  EXPECT_EQ(raw_port->client_, session.get());
}

TEST_F(ReceiverSessionTest, CanNegotiateWithDefaultPreferences) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  SimpleMessagePort* raw_port = message_port.get();
  StrictMock<FakeClient> client;
  ReceiverSession session(&client, std::move(message_port), std::move(env_),
                          ReceiverSession::Preferences{});

  EXPECT_CALL(client, OnNegotiated(&session, _))
      .WillOnce([](ReceiverSession* session,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_TRUE(cr.audio_receiver());
        EXPECT_TRUE(cr.audio_session_config());
        EXPECT_EQ(cr.audio_session_config().value().sender_ssrc, 19088747u);
        EXPECT_EQ(cr.audio_session_config().value().receiver_ssrc, 19088748u);
        EXPECT_EQ(cr.audio_session_config().value().channels, 2);
        EXPECT_EQ(cr.audio_session_config().value().rtp_timebase, 48000);

        EXPECT_TRUE(cr.video_receiver());
        EXPECT_TRUE(cr.video_session_config());
        // We should have chosen vp8
        EXPECT_EQ(cr.video_session_config().value().sender_ssrc, 19088745u);
        EXPECT_EQ(cr.video_session_config().value().receiver_ssrc, 19088746u);
        EXPECT_EQ(cr.video_session_config().value().channels, 1);
        EXPECT_EQ(cr.video_session_config().value().rtp_timebase, 90000);
      });

  raw_port->ReceiveMessage(kValidOfferMessage);
}

TEST_F(ReceiverSessionTest, CanNegotiateWithCustomPreferences) {
  auto message_port = std::make_unique<SimpleMessagePort>();
  SimpleMessagePort* raw_port = message_port.get();
  StrictMock<FakeClient> client;
  ReceiverSession session(
      &client, std::move(message_port), std::move(env_),
      ReceiverSession::Preferences{{ReceiverSession::VideoCodec::kVp9},
                                   {ReceiverSession::AudioCodec::kOpus}});

  EXPECT_CALL(client, OnNegotiated(&session, _))
      .WillOnce([](ReceiverSession* session,
                   ReceiverSession::ConfiguredReceivers cr) {
        EXPECT_TRUE(cr.audio_receiver());
        EXPECT_TRUE(cr.audio_session_config());
        EXPECT_EQ(cr.audio_session_config().value().sender_ssrc, 19088747u);
        EXPECT_EQ(cr.audio_session_config().value().receiver_ssrc, 19088748u);
        EXPECT_EQ(cr.audio_session_config().value().channels, 2);
        EXPECT_EQ(cr.audio_session_config().value().rtp_timebase, 48000);

        EXPECT_TRUE(cr.video_receiver());
        EXPECT_TRUE(cr.video_session_config());
        // We should have chosen vp9
        EXPECT_EQ(cr.video_session_config().value().sender_ssrc, 19088743u);
        EXPECT_EQ(cr.video_session_config().value().receiver_ssrc, 19088744u);
        EXPECT_EQ(cr.video_session_config().value().channels, 1);
        EXPECT_EQ(cr.video_session_config().value().rtp_timebase, 90000);
      });
  ;
  raw_port->ReceiveMessage(kValidOfferMessage);
}
}  // namespace streaming
}  // namespace cast
