// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_remoting_sender.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/cast/cast_environment.h"
#include "media/cast/constants.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_impl.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/test/utility/default_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast {

namespace {

// Number of bytes in each faked frame.
constexpr int kFrameSize = 5000;

class TestPacketSender : public media::cast::PacketTransport {
 public:
  TestPacketSender()
      : number_of_rtp_packets_(0), number_of_rtcp_packets_(0), paused_(false) {}

  // A singular packet implies a RTCP packet.
  bool SendPacket(media::cast::PacketRef packet,
                  const base::Closure& cb) final {
    if (paused_) {
      stored_packet_ = packet;
      callback_ = cb;
      return false;
    }
    if (media::cast::IsRtcpPacket(&packet->data[0], packet->data.size())) {
      ++number_of_rtcp_packets_;
    } else {
      ++number_of_rtp_packets_;
    }
    return true;
  }

  int64_t GetBytesSent() final { return 0; }

  void StartReceiving(const media::cast::PacketReceiverCallbackWithStatus&
                          packet_receiver) final {}

  void StopReceiving() final {}

  int number_of_rtp_packets() const { return number_of_rtp_packets_; }

  int number_of_rtcp_packets() const { return number_of_rtcp_packets_; }

  void SetPause(bool paused) {
    paused_ = paused;
    if (!paused && stored_packet_.get()) {
      SendPacket(stored_packet_, callback_);
      callback_.Run();
    }
  }

 private:
  int number_of_rtp_packets_;
  int number_of_rtcp_packets_;
  bool paused_;
  base::Closure callback_;
  media::cast::PacketRef stored_packet_;

  DISALLOW_COPY_AND_ASSIGN(TestPacketSender);
};

class DummyClient : public media::cast::CastTransport::Client {
 public:
  DummyClient() {}

  void OnStatusChanged(media::cast::CastTransportStatus status) final {
    EXPECT_EQ(media::cast::TRANSPORT_STREAM_INITIALIZED, status);
  };
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<media::cast::FrameEvent>> frame_events,
      std::unique_ptr<std::vector<media::cast::PacketEvent>> packet_events)
      final {}
  void ProcessRtpPacket(std::unique_ptr<media::cast::Packet> packet) final {}

  DISALLOW_COPY_AND_ASSIGN(DummyClient);
};

}  // namespace

class CastRemotingSenderTest : public ::testing::Test {
 protected:
  CastRemotingSenderTest()
      : testing_clock_(new base::SimpleTestTickClock()),
        task_runner_(new media::FakeSingleThreadTaskRunner(testing_clock_)),
        cast_environment_(
            new media::cast::CastEnvironment(base::WrapUnique(testing_clock_),
                                             task_runner_,
                                             task_runner_,
                                             task_runner_)) {
    testing_clock_->Advance(base::TimeTicks::Now() - base::TimeTicks());
    transport_ = new TestPacketSender();
    transport_sender_.reset(new media::cast::CastTransportImpl(
        testing_clock_, base::TimeDelta(), base::MakeUnique<DummyClient>(),
        base::WrapUnique(transport_), task_runner_));
    media::cast::FrameSenderConfig video_config =
        media::cast::GetDefaultVideoSenderConfig();
    video_config.rtp_payload_type = media::cast::RtpPayloadType::REMOTE_VIDEO;
    media::cast::CastTransportRtpConfig transport_config;
    transport_config.ssrc = video_config.sender_ssrc;
    transport_config.rtp_stream_id = 5;
    transport_config.feedback_ssrc = video_config.receiver_ssrc;
    transport_config.rtp_payload_type = video_config.rtp_payload_type;
    transport_config.aes_key = video_config.aes_key;
    transport_config.aes_iv_mask = video_config.aes_iv_mask;
    remoting_sender_.reset(new CastRemotingSender(
        cast_environment_, transport_sender_.get(), transport_config));
    task_runner_->RunTasks();
  }

  ~CastRemotingSenderTest() override {}

  void TearDown() final {
    remoting_sender_.reset();
    task_runner_->RunTasks();
  }

  void InsertFrame() {
    remoting_sender_->next_frame_data_.resize(kFrameSize);
    remoting_sender_->SendFrame();
  }

  void OnReceivedCastMessage(
      const media::cast::RtcpCastMessage& cast_feedback) {
    remoting_sender_->OnReceivedCastMessage(cast_feedback);
  }

  int NumberOfFramesInFlight() {
    return remoting_sender_->NumberOfFramesInFlight();
  }

  void CancelFramesInFlight() { remoting_sender_->CancelFramesInFlight(); }

  base::SimpleTestTickClock* const testing_clock_;  // Owned by CastEnvironment.
  const scoped_refptr<media::FakeSingleThreadTaskRunner> task_runner_;
  const scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  TestPacketSender* transport_;  // Owned by CastTransport.
  std::unique_ptr<media::cast::CastTransportImpl> transport_sender_;
  std::unique_ptr<CastRemotingSender> remoting_sender_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastRemotingSenderTest);
};

TEST_F(CastRemotingSenderTest, SendAndAckEncodedFrame) {
  // Send a fake video frame that will be decomposed into 4 packets.
  InsertFrame();
  task_runner_->RunTasks();
  EXPECT_EQ(4, transport_->number_of_rtp_packets());
  EXPECT_EQ(1, NumberOfFramesInFlight());

  // Ack the frame.
  media::cast::RtcpCastMessage cast_feedback(11);
  cast_feedback.ack_frame_id = media::cast::FrameId::first();
  OnReceivedCastMessage(cast_feedback);
  EXPECT_EQ(0, NumberOfFramesInFlight());

  // Send 4 frames.
  for (int i = 0; i < 4; ++i)
    InsertFrame();
  EXPECT_EQ(4, NumberOfFramesInFlight());
  // Ack one frame.
  cast_feedback.ack_frame_id = media::cast::FrameId::first() + 1;
  OnReceivedCastMessage(cast_feedback);
  EXPECT_EQ(3, NumberOfFramesInFlight());
  // Ack all.
  cast_feedback.received_later_frames.clear();
  cast_feedback.ack_frame_id = media::cast::FrameId::first() + 4;
  OnReceivedCastMessage(cast_feedback);
  EXPECT_EQ(0, NumberOfFramesInFlight());
}

TEST_F(CastRemotingSenderTest, CancelFramesInFlight) {
  for (int i = 0; i < 10; ++i)
    InsertFrame();
  EXPECT_EQ(10, NumberOfFramesInFlight());

  media::cast::RtcpCastMessage cast_feedback(11);
  cast_feedback.ack_frame_id = media::cast::FrameId::first();
  OnReceivedCastMessage(cast_feedback);
  EXPECT_EQ(9, NumberOfFramesInFlight());

  CancelFramesInFlight();
  EXPECT_EQ(0, NumberOfFramesInFlight());
}

}  // namespace cast
