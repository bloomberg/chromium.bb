// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/media/cast_transport_host_filter.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CastTransportHostFilterTest : public testing::Test {
 public:
  CastTransportHostFilterTest()
      : browser_thread_bundle_(
            content::TestBrowserThreadBundle::IO_MAINLOOP) {
    filter_ = new cast::CastTransportHostFilter();
    local_endpoint_ = net::IPEndPoint(net::IPAddressNumber(4, 0), 0);
    // 127.0.0.1:7 is the local echo service port, which
    // is probably not going to respond, but that's ok.
    // TODO(hubbe): Open up an UDP port and make sure
    // we can send and receive packets.
    net::IPAddressNumber receiver_address(4, 0);
    receiver_address[0] = 127;
    receiver_address[3] = 1;
    receive_endpoint_ = net::IPEndPoint(receiver_address, 7);
  }

 protected:
  void FakeSend(const IPC::Message& message) {
    bool message_was_ok;
    EXPECT_TRUE(filter_->OnMessageReceived(message, &message_was_ok));
    EXPECT_TRUE(message_was_ok);
  }

  content::TestBrowserThreadBundle browser_thread_bundle_;
  scoped_refptr<content::BrowserMessageFilter> filter_;
  net::IPAddressNumber receiver_address_;
  net::IPEndPoint local_endpoint_;
  net::IPEndPoint receive_endpoint_;
};

TEST_F(CastTransportHostFilterTest, NewDelete) {
  const int kChannelId = 17;
  CastHostMsg_New new_msg(kChannelId, local_endpoint_, receive_endpoint_);
  CastHostMsg_Delete delete_msg(kChannelId);

  // New, then delete, as expected.
  FakeSend(new_msg);
  FakeSend(delete_msg);
  FakeSend(new_msg);
  FakeSend(delete_msg);
  FakeSend(new_msg);
  FakeSend(delete_msg);

  // Now create/delete transport senders in the wrong order to make sure
  // this doesn't crash.
  FakeSend(new_msg);
  FakeSend(new_msg);
  FakeSend(new_msg);
  FakeSend(delete_msg);
  FakeSend(delete_msg);
  FakeSend(delete_msg);
}

TEST_F(CastTransportHostFilterTest, NewMany) {
  for (int i = 0; i < 100; i++) {
    CastHostMsg_New new_msg(i, local_endpoint_, receive_endpoint_);
    FakeSend(new_msg);
  }

  for (int i = 0; i < 60; i++) {
    CastHostMsg_Delete delete_msg(i);
    FakeSend(delete_msg);
  }

  // Leave some open, see what happens.
}

TEST_F(CastTransportHostFilterTest, SimpleMessages) {
  // Create a cast transport sender.
  const int32 kChannelId = 42;
  CastHostMsg_New new_msg(kChannelId, local_endpoint_, receive_endpoint_);
  FakeSend(new_msg);

  media::cast::transport::CastTransportAudioConfig audio_config;
  CastHostMsg_InitializeAudio init_audio_msg(kChannelId, audio_config);
  FakeSend(init_audio_msg);

  media::cast::transport::CastTransportVideoConfig video_config;
  CastHostMsg_InitializeVideo init_video_msg(kChannelId, video_config);
  FakeSend(init_video_msg);
  media::cast::transport::EncodedAudioFrame audio_frame;
  audio_frame.codec = media::cast::transport::kPcm16;
  audio_frame.frame_id = 1;
  audio_frame.rtp_timestamp = 47;
  const int kSamples = 47;
  const int kBytesPerSample = 2;
  const int kChannels = 2;
  audio_frame.data = std::string(kSamples * kBytesPerSample * kChannels, 'q');
  CastHostMsg_InsertCodedAudioFrame insert_coded_audio_frame(
      kChannelId, audio_frame, base::TimeTicks::Now());
  FakeSend(insert_coded_audio_frame);

  media::cast::transport::EncodedVideoFrame video_frame;
  video_frame.codec = media::cast::transport::kVp8;
  video_frame.key_frame = true;
  video_frame.frame_id = 1;
  video_frame.last_referenced_frame_id = 0;
  // Let's make sure we try a few kb so multiple packets
  // are generated.
  const int kVideoDataSize = 4711;
  video_frame.data = std::string(kVideoDataSize, 'p');
  CastHostMsg_InsertCodedVideoFrame insert_coded_video_frame(
      kChannelId, video_frame, base::TimeTicks::Now());
  FakeSend(insert_coded_video_frame);

  media::cast::transport::SendRtcpFromRtpSenderData rtcp_data;
  rtcp_data.packet_type_flags = 0;
  rtcp_data.sending_ssrc = 0;
  rtcp_data.c_name = "FNRD";
  media::cast::transport::RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = 1;
  sender_info.ntp_fraction = 2;
  sender_info.rtp_timestamp = 3;
  sender_info.send_packet_count = 4;
  sender_info.send_octet_count = 5;
  media::cast::transport::RtcpDlrrReportBlock dlrr;
  dlrr.last_rr = 7;
  dlrr.delay_since_last_rr = 8;
  media::cast::transport::RtcpSenderLogMessage sender_log(1);
  sender_log[0].frame_status =
      media::cast::transport::kRtcpSenderFrameStatusSentToNetwork;
  sender_log[0].rtp_timestamp = 9;
  CastHostMsg_SendRtcpFromRtpSender rtcp_msg(
      kChannelId, rtcp_data, sender_info, dlrr, sender_log);
  FakeSend(rtcp_msg);

  media::cast::MissingFramesAndPacketsMap missing_packets;
  missing_packets[1].insert(4);
  missing_packets[3].insert(7);
  CastHostMsg_ResendPackets resend_msg(
      kChannelId, false, missing_packets);
  FakeSend(resend_msg);

  CastHostMsg_Delete delete_msg(kChannelId);
  FakeSend(delete_msg);
}

}  // namespace
