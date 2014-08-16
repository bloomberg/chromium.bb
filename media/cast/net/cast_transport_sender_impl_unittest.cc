// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/cast_config.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_sender_impl.h"
#include "media/cast/net/rtcp/rtcp.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {
const int64 kStartMillisecond = INT64_C(12345678900000);
const uint32 kVideoSsrc = 1;
}  // namespace

class FakePacketSender : public PacketSender {
 public:
  FakePacketSender()
      : paused_(false), packets_sent_(0) {}

  virtual bool SendPacket(PacketRef packet, const base::Closure& cb) OVERRIDE {
    if (paused_) {
      stored_packet_ = packet;
      callback_ = cb;
      return false;
    }
    ++packets_sent_;
    return true;
  }

  void SetPaused(bool paused) {
    paused_ = paused;
    if (!paused && stored_packet_) {
      SendPacket(stored_packet_, callback_);
      callback_.Run();
    }
  }

  int packets_sent() const { return packets_sent_; }

 private:
  bool paused_;
  base::Closure callback_;
  PacketRef stored_packet_;
  int packets_sent_;

  DISALLOW_COPY_AND_ASSIGN(FakePacketSender);
};

class CastTransportSenderImplTest : public ::testing::Test {
 protected:
  CastTransportSenderImplTest()
      : num_times_callback_called_(0) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    task_runner_ = new test::FakeSingleThreadTaskRunner(&testing_clock_);
  }

  virtual ~CastTransportSenderImplTest() {}

  void InitWithoutLogging() {
    transport_sender_.reset(
        new CastTransportSenderImpl(NULL,
                                    &testing_clock_,
                                    net::IPEndPoint(),
                                    base::Bind(&UpdateCastTransportStatus),
                                    BulkRawEventsCallback(),
                                    base::TimeDelta(),
                                    task_runner_,
                                    &transport_));
    task_runner_->RunTasks();
  }

  void InitWithLogging() {
    transport_sender_.reset(new CastTransportSenderImpl(
        NULL,
        &testing_clock_,
        net::IPEndPoint(),
        base::Bind(&UpdateCastTransportStatus),
        base::Bind(&CastTransportSenderImplTest::LogRawEvents,
                   base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(10),
        task_runner_,
        &transport_));
    task_runner_->RunTasks();
  }

  void InitializeVideo() {
    CastTransportRtpConfig rtp_config;
    rtp_config.ssrc = kVideoSsrc;
    rtp_config.feedback_ssrc = 2;
    rtp_config.rtp_payload_type = 3;
    rtp_config.stored_frames = 10;
    transport_sender_->InitializeVideo(rtp_config,
                                       RtcpCastMessageCallback(),
                                       RtcpRttCallback());

  }

  void LogRawEvents(const std::vector<PacketEvent>& packet_events,
                    const std::vector<FrameEvent>& frame_events) {
    num_times_callback_called_++;
  }

  static void UpdateCastTransportStatus(CastTransportStatus status) {
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<CastTransportSenderImpl> transport_sender_;
  FakePacketSender transport_;
  int num_times_callback_called_;
};

TEST_F(CastTransportSenderImplTest, InitWithoutLogging) {
  InitWithoutLogging();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(50));
  EXPECT_EQ(0, num_times_callback_called_);
}

TEST_F(CastTransportSenderImplTest, InitWithLogging) {
  InitWithLogging();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(50));
  EXPECT_EQ(5, num_times_callback_called_);
}

TEST_F(CastTransportSenderImplTest, NacksCancelRetransmits) {
  InitWithoutLogging();
  InitializeVideo();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(50));

  // A fake frame that will be decomposed into 4 packets.
  EncodedFrame fake_frame;
  fake_frame.frame_id = 1;
  fake_frame.rtp_timestamp = 1;
  fake_frame.dependency = EncodedFrame::KEY;
  fake_frame.data.resize(5000, ' ');

  transport_sender_->InsertCodedVideoFrame(fake_frame);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(4, transport_.packets_sent());

  // Resend packet 0.
  MissingFramesAndPacketsMap missing_packets;
  missing_packets[1].insert(0);
  missing_packets[1].insert(1);
  missing_packets[1].insert(2);

  transport_.SetPaused(true);
  transport_sender_->ResendPackets(
      kVideoSsrc, missing_packets, true,
      base::TimeDelta::FromMilliseconds(10));

  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));

  RtcpCastMessage cast_message;
  cast_message.media_ssrc = kVideoSsrc;
  cast_message.ack_frame_id = 1;
  cast_message.missing_frames_and_packets[1].insert(3);
  transport_sender_->OnReceivedCastMessage(kVideoSsrc,
                                           RtcpCastMessageCallback(),
                                           cast_message);
  transport_.SetPaused(false);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));

  // Resend one packet in the socket when unpaused.
  // Resend one more packet from NACK.
  EXPECT_EQ(6, transport_.packets_sent());
}

TEST_F(CastTransportSenderImplTest, CancelRetransmits) {
  InitWithoutLogging();
  InitializeVideo();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(50));

  // A fake frame that will be decomposed into 4 packets.
  EncodedFrame fake_frame;
  fake_frame.frame_id = 1;
  fake_frame.rtp_timestamp = 1;
  fake_frame.dependency = EncodedFrame::KEY;
  fake_frame.data.resize(5000, ' ');

  transport_sender_->InsertCodedVideoFrame(fake_frame);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(4, transport_.packets_sent());

  // Resend all packets for frame 1.
  MissingFramesAndPacketsMap missing_packets;
  missing_packets[1].insert(kRtcpCastAllPacketsLost);

  transport_.SetPaused(true);
  transport_sender_->ResendPackets(
      kVideoSsrc, missing_packets, true,
      base::TimeDelta::FromMilliseconds(10));

  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  std::vector<uint32> cancel_sending_frames;
  cancel_sending_frames.push_back(1);
  transport_sender_->CancelSendingFrames(kVideoSsrc,
                                         cancel_sending_frames);
  transport_.SetPaused(false);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));

  // Resend one packet in the socket when unpaused.
  EXPECT_EQ(5, transport_.packets_sent());
}

TEST_F(CastTransportSenderImplTest, Kickstart) {
  InitWithoutLogging();
  InitializeVideo();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(50));

  // A fake frame that will be decomposed into 4 packets.
  EncodedFrame fake_frame;
  fake_frame.frame_id = 1;
  fake_frame.rtp_timestamp = 1;
  fake_frame.dependency = EncodedFrame::KEY;
  fake_frame.data.resize(5000, ' ');

  transport_.SetPaused(true);
  transport_sender_->InsertCodedVideoFrame(fake_frame);
  transport_sender_->ResendFrameForKickstart(kVideoSsrc, 1);
  transport_.SetPaused(false);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(4, transport_.packets_sent());

  // Resend 2 packets for frame 1.
  MissingFramesAndPacketsMap missing_packets;
  missing_packets[1].insert(0);
  missing_packets[1].insert(1);

  transport_.SetPaused(true);
  transport_sender_->ResendPackets(
      kVideoSsrc, missing_packets, true,
      base::TimeDelta::FromMilliseconds(10));
  transport_sender_->ResendFrameForKickstart(kVideoSsrc, 1);
  transport_.SetPaused(false);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));

  // Resend one packet in the socket when unpaused.
  // Two more retransmission packets sent.
  EXPECT_EQ(7, transport_.packets_sent());
}

}  // namespace cast
}  // namespace media
