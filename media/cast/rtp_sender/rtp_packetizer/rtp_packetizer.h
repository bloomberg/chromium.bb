// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_SENDER_RTP_PACKETIZER_RTP_PACKETIZER_H_
#define MEDIA_CAST_RTP_SENDER_RTP_PACKETIZER_RTP_PACKETIZER_H_

#include <cmath>
#include <list>
#include <map>

#include "media/cast/rtp_common/rtp_defines.h"
#include "media/cast/rtp_sender/packet_storage/packet_storage.h"
#include "media/cast/rtp_sender/rtp_packetizer/rtp_packetizer_config.h"

namespace media {
namespace cast {

class PacedPacketSender;

class RtpPacketizer {
 public:
  RtpPacketizer(PacedPacketSender* transport,
                PacketStorage* packet_storage,
                RtpPacketizerConfig rtp_packetizer_config);
  ~RtpPacketizer();

  void IncomingEncodedVideoFrame(const EncodedVideoFrame& video_frame,
                                 int64 capture_time);

  void IncomingEncodedAudioFrame(const EncodedAudioFrame& audio_frame,
                                 int64 recorded_time);

  bool LastSentTimestamp(int64* time_sent, uint32* rtp_timestamp) const;

  // Return the next sequence number, and increment by one. Enables unique
  // incremental sequence numbers for every packet (including retransmissions).
  uint16 NextSequenceNumber();

  uint32 send_packets_count() {return send_packets_count_;}
  uint32 send_octet_count() {return send_octet_count_;}

 private:
  void Cast(bool is_key, uint8 reference_frame_id,
    uint32 timestamp, std::vector<uint8> data);
  void BuildCommonRTPheader(std::vector<uint8>* packet, bool marker_bit,
      uint32 time_stamp);
  RtpPacketizerConfig config_;
  PacedPacketSender* transport_;
  PacketStorage* packet_storage_;

  int64 time_last_sent_rtp_timestamp_;
  uint16 sequence_number_;
  uint32 rtp_timestamp_;
  uint8 frame_id_;
  uint16 packet_id_;

  uint32 send_packets_count_;
  uint32 send_octet_count_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_SENDER_RTP_PACKETIZER_RTP_PACKETIZER_H_
