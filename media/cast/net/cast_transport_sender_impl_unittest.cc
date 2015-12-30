// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/cast_transport_sender_impl.h"

#include <gtest/gtest.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/values.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {
const int64_t kStartMillisecond = INT64_C(12345678900000);
const uint32_t kVideoSsrc = 1;
const uint32_t kAudioSsrc = 2;
}  // namespace

class FakePacketSender : public PacketSender {
 public:
  FakePacketSender()
      : paused_(false), packets_sent_(0), bytes_sent_(0) {}

  bool SendPacket(PacketRef packet, const base::Closure& cb) final {
    if (paused_) {
      stored_packet_ = packet;
      callback_ = cb;
      return false;
    }
    ++packets_sent_;
    bytes_sent_ += packet->data.size();
    return true;
  }

  int64_t GetBytesSent() final { return bytes_sent_; }

  void SetPaused(bool paused) {
    paused_ = paused;
    if (!paused && stored_packet_.get()) {
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
  int64_t bytes_sent_;

  DISALLOW_COPY_AND_ASSIGN(FakePacketSender);
};

class CastTransportSenderImplTest : public ::testing::Test {
 protected:
  CastTransportSenderImplTest() : num_times_logging_callback_called_(0) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    task_runner_ = new test::FakeSingleThreadTaskRunner(&testing_clock_);
  }

  ~CastTransportSenderImplTest() override {}

  void InitWithoutLogging() {
    transport_sender_.reset(
        new CastTransportSenderImpl(NULL,
                                    &testing_clock_,
                                    net::IPEndPoint(),
                                    net::IPEndPoint(),
                                    make_scoped_ptr(new base::DictionaryValue),
                                    base::Bind(&UpdateCastTransportStatus),
                                    BulkRawEventsCallback(),
                                    base::TimeDelta(),
                                    task_runner_,
                                    PacketReceiverCallback(),
                                    &transport_));
    task_runner_->RunTasks();
  }

  void InitWithOptions() {
    scoped_ptr<base::DictionaryValue> options(
        new base::DictionaryValue);
    options->SetBoolean("DHCP", true);
    options->SetBoolean("disable_wifi_scan", true);
    options->SetBoolean("media_streaming_mode", true);
    options->SetInteger("pacer_target_burst_size", 20);
    options->SetInteger("pacer_max_burst_size", 100);
    transport_sender_.reset(new CastTransportSenderImpl(
        NULL, &testing_clock_, net::IPEndPoint(), net::IPEndPoint(),
        std::move(options), base::Bind(&UpdateCastTransportStatus),
        BulkRawEventsCallback(), base::TimeDelta(), task_runner_,
        PacketReceiverCallback(), &transport_));
    task_runner_->RunTasks();
  }

  void InitWithLogging() {
    transport_sender_.reset(new CastTransportSenderImpl(
        NULL,
        &testing_clock_,
        net::IPEndPoint(),
        net::IPEndPoint(),
        make_scoped_ptr(new base::DictionaryValue),
        base::Bind(&UpdateCastTransportStatus),
        base::Bind(&CastTransportSenderImplTest::LogRawEvents,
                   base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(10),
        task_runner_,
        PacketReceiverCallback(),
        &transport_));
    task_runner_->RunTasks();
  }

  void InitializeVideo() {
    CastTransportRtpConfig rtp_config;
    rtp_config.ssrc = kVideoSsrc;
    rtp_config.feedback_ssrc = 2;
    rtp_config.rtp_payload_type = 3;
    transport_sender_->InitializeVideo(rtp_config,
                                       RtcpCastMessageCallback(),
                                       RtcpRttCallback());
  }

  void InitializeAudio() {
    CastTransportRtpConfig rtp_config;
    rtp_config.ssrc = kAudioSsrc;
    rtp_config.feedback_ssrc = 3;
    rtp_config.rtp_payload_type = 4;
    transport_sender_->InitializeAudio(rtp_config,
                                       RtcpCastMessageCallback(),
                                       RtcpRttCallback());
  }

  void LogRawEvents(scoped_ptr<std::vector<FrameEvent>> frame_events,
                    scoped_ptr<std::vector<PacketEvent>> packet_events) {
    num_times_logging_callback_called_++;
  }

  static void UpdateCastTransportStatus(CastTransportStatus status) {
  }

  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner_;
  scoped_ptr<CastTransportSenderImpl> transport_sender_;
  FakePacketSender transport_;
  int num_times_logging_callback_called_;
};

TEST_F(CastTransportSenderImplTest, InitWithoutLogging) {
  InitWithoutLogging();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);
}

TEST_F(CastTransportSenderImplTest, InitWithOptions) {
  InitWithOptions();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);
}

TEST_F(CastTransportSenderImplTest, NacksCancelRetransmits) {
  InitWithLogging();
  InitializeVideo();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);

  // A fake frame that will be decomposed into 4 packets.
  EncodedFrame fake_frame;
  fake_frame.frame_id = 1;
  fake_frame.rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(1));
  fake_frame.dependency = EncodedFrame::KEY;
  fake_frame.data.resize(5000, ' ');

  transport_sender_->InsertFrame(kVideoSsrc, fake_frame);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(4, transport_.packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);

  // Resend packet 0.
  MissingFramesAndPacketsMap missing_packets;
  missing_packets[1].insert(0);
  missing_packets[1].insert(1);
  missing_packets[1].insert(2);

  transport_.SetPaused(true);
  DedupInfo dedup_info;
  dedup_info.resend_interval = base::TimeDelta::FromMilliseconds(10);
  transport_sender_->ResendPackets(
      kVideoSsrc, missing_packets, true, dedup_info);

  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(2, num_times_logging_callback_called_);

  RtcpCastMessage cast_message;
  cast_message.media_ssrc = kVideoSsrc;
  cast_message.ack_frame_id = 1;
  cast_message.missing_frames_and_packets[1].insert(3);
  transport_sender_->OnReceivedCastMessage(kVideoSsrc,
                                           RtcpCastMessageCallback(),
                                           cast_message);
  transport_.SetPaused(false);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(3, num_times_logging_callback_called_);

  // Resend one packet in the socket when unpaused.
  // Resend one more packet from NACK.
  EXPECT_EQ(6, transport_.packets_sent());
}

TEST_F(CastTransportSenderImplTest, CancelRetransmits) {
  InitWithLogging();
  InitializeVideo();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);

  // A fake frame that will be decomposed into 4 packets.
  EncodedFrame fake_frame;
  fake_frame.frame_id = 1;
  fake_frame.rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(1));
  fake_frame.dependency = EncodedFrame::KEY;
  fake_frame.data.resize(5000, ' ');

  transport_sender_->InsertFrame(kVideoSsrc, fake_frame);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(4, transport_.packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);

  // Resend all packets for frame 1.
  MissingFramesAndPacketsMap missing_packets;
  missing_packets[1].insert(kRtcpCastAllPacketsLost);

  transport_.SetPaused(true);
  DedupInfo dedup_info;
  dedup_info.resend_interval = base::TimeDelta::FromMilliseconds(10);
  transport_sender_->ResendPackets(
      kVideoSsrc, missing_packets, true, dedup_info);

  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(2, num_times_logging_callback_called_);

  std::vector<uint32_t> cancel_sending_frames;
  cancel_sending_frames.push_back(1);
  transport_sender_->CancelSendingFrames(kVideoSsrc,
                                         cancel_sending_frames);
  transport_.SetPaused(false);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(2, num_times_logging_callback_called_);

  // Resend one packet in the socket when unpaused.
  EXPECT_EQ(5, transport_.packets_sent());
}

TEST_F(CastTransportSenderImplTest, Kickstart) {
  InitWithLogging();
  InitializeVideo();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);

  // A fake frame that will be decomposed into 4 packets.
  EncodedFrame fake_frame;
  fake_frame.frame_id = 1;
  fake_frame.rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(1));
  fake_frame.dependency = EncodedFrame::KEY;
  fake_frame.data.resize(5000, ' ');

  transport_.SetPaused(true);
  transport_sender_->InsertFrame(kVideoSsrc, fake_frame);
  transport_sender_->ResendFrameForKickstart(kVideoSsrc, 1);
  transport_.SetPaused(false);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(4, transport_.packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);

  // Resend 2 packets for frame 1.
  MissingFramesAndPacketsMap missing_packets;
  missing_packets[1].insert(0);
  missing_packets[1].insert(1);

  transport_.SetPaused(true);
  DedupInfo dedup_info;
  dedup_info.resend_interval = base::TimeDelta::FromMilliseconds(10);
  transport_sender_->ResendPackets(
      kVideoSsrc, missing_packets, true, dedup_info);
  transport_sender_->ResendFrameForKickstart(kVideoSsrc, 1);
  transport_.SetPaused(false);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(2, num_times_logging_callback_called_);

  // Resend one packet in the socket when unpaused.
  // Two more retransmission packets sent.
  EXPECT_EQ(7, transport_.packets_sent());
}

TEST_F(CastTransportSenderImplTest, DedupRetransmissionWithAudio) {
  InitWithLogging();
  InitializeAudio();
  InitializeVideo();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(50));
  EXPECT_EQ(0, num_times_logging_callback_called_);

  // Send two audio frames.
  EncodedFrame fake_audio;
  fake_audio.frame_id = 1;
  fake_audio.reference_time = testing_clock_.NowTicks();
  fake_audio.dependency = EncodedFrame::KEY;
  fake_audio.data.resize(100, ' ');
  transport_sender_->InsertFrame(kAudioSsrc, fake_audio);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(2));
  fake_audio.frame_id = 2;
  fake_audio.reference_time = testing_clock_.NowTicks();
  transport_sender_->InsertFrame(kAudioSsrc, fake_audio);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(2));
  EXPECT_EQ(2, transport_.packets_sent());

  // Ack the first audio frame.
  RtcpCastMessage cast_message;
  cast_message.media_ssrc = kAudioSsrc;
  cast_message.ack_frame_id = 1;
  transport_sender_->OnReceivedCastMessage(kAudioSsrc,
                                           RtcpCastMessageCallback(),
                                           cast_message);
  task_runner_->RunTasks();
  EXPECT_EQ(2, transport_.packets_sent());
  EXPECT_EQ(0, num_times_logging_callback_called_);  // Only 4 ms since last.

  // Send a fake video frame that will be decomposed into 4 packets.
  EncodedFrame fake_video;
  fake_video.frame_id = 1;
  fake_video.dependency = EncodedFrame::KEY;
  fake_video.data.resize(5000, ' ');
  transport_sender_->InsertFrame(kVideoSsrc, fake_video);
  task_runner_->RunTasks();
  EXPECT_EQ(6, transport_.packets_sent());
  EXPECT_EQ(0, num_times_logging_callback_called_);  // Only 4 ms since last.

  // Retransmission is reject because audio is not acked yet.
  cast_message.media_ssrc = kVideoSsrc;
  cast_message.ack_frame_id = 0;
  cast_message.missing_frames_and_packets[1].insert(3);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(10));
  transport_sender_->OnReceivedCastMessage(kVideoSsrc,
                                           RtcpCastMessageCallback(),
                                           cast_message);
  task_runner_->RunTasks();
  EXPECT_EQ(6, transport_.packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);

  // Ack the second audio frame.
  cast_message.media_ssrc = kAudioSsrc;
  cast_message.ack_frame_id = 2;
  cast_message.missing_frames_and_packets.clear();
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(2));
  transport_sender_->OnReceivedCastMessage(kAudioSsrc,
                                           RtcpCastMessageCallback(),
                                           cast_message);
  task_runner_->RunTasks();
  EXPECT_EQ(6, transport_.packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);  // Only 6 ms since last.

  // Retransmission of video packet now accepted.
  cast_message.media_ssrc = kVideoSsrc;
  cast_message.ack_frame_id = 1;
  cast_message.missing_frames_and_packets[1].insert(3);
  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(2));
  transport_sender_->OnReceivedCastMessage(kVideoSsrc,
                                           RtcpCastMessageCallback(),
                                           cast_message);
  task_runner_->RunTasks();
  EXPECT_EQ(7, transport_.packets_sent());
  EXPECT_EQ(1, num_times_logging_callback_called_);  // Only 8 ms since last.

  task_runner_->Sleep(base::TimeDelta::FromMilliseconds(2));
  EXPECT_EQ(2, num_times_logging_callback_called_);
}

}  // namespace cast
}  // namespace media
