// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the interface to the cast RTP sender.

#ifndef MEDIA_CAST_RTP_SENDER_RTP_SENDER_H_
#define MEDIA_CAST_RTP_SENDER_RTP_SENDER_H_

#include <map>
#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtp_sender/packet_storage/packet_storage.h"
#include "media/cast/rtp_sender/rtp_packetizer/rtp_packetizer.h"
#include "media/cast/rtp_sender/rtp_packetizer/rtp_packetizer_config.h"

namespace media {
namespace cast {

class PacedPacketSender;
struct RtcpSenderInfo;

typedef std::map<uint8, std::set<uint16> > MissingFramesAndPackets;

class RtpSender {
 public:
  RtpSender(const AudioSenderConfig* audio_config,
            const VideoSenderConfig* video_config,
            PacedPacketSender* transport);

  ~RtpSender();

  void IncomingEncodedVideoFrame(const EncodedVideoFrame& video_frame,
                                 int64 capture_time);

  void IncomingEncodedAudioFrame(const EncodedAudioFrame& audio_frame,
                                 int64 recorded_time);

  void ResendPackets(
      const MissingFramesAndPackets& missing_frames_and_packets);

  void RtpStatistics(int64 now_ms, RtcpSenderInfo* sender_info);

 private:
  void UpdateSequenceNumber(std::vector<uint8>* packet);

  RtpPacketizerConfig config_;
  scoped_ptr<RtpPacketizer> packetizer_;
  scoped_ptr<PacketStorage> storage_;
  PacedPacketSender* transport_;
  scoped_ptr<base::TickClock> default_tick_clock_;
  base::TickClock* clock_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_SENDER_RTP_SENDER_H_
