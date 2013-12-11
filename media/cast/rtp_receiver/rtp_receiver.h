// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface to the rtp receiver.

#ifndef MEDIA_CAST_RTP_RECEIVER_RTP_RECEIVER_H_
#define MEDIA_CAST_RTP_RECEIVER_RTP_RECEIVER_H_

#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

class RtpData {
 public:
  virtual void OnReceivedPayloadData(const uint8* payload_data,
                                     size_t payload_size,
                                     const RtpCastHeader* rtp_header) = 0;

 protected:
  virtual ~RtpData() {}
};

class ReceiverStats;
class RtpParser;

class RtpReceiver {
 public:
  RtpReceiver(base::TickClock* clock,
              const AudioReceiverConfig* audio_config,
              const VideoReceiverConfig* video_config,
              RtpData* incoming_payload_callback);
  ~RtpReceiver();

  static uint32 GetSsrcOfSender(const uint8* rtcp_buffer, size_t length);

  bool ReceivedPacket(const uint8* packet, size_t length);

  void GetStatistics(uint8* fraction_lost,
                     uint32* cumulative_lost,  // 24 bits valid.
                     uint32* extended_high_sequence_number,
                     uint32* jitter);

 private:
  scoped_ptr<ReceiverStats> stats_;
  scoped_ptr<RtpParser> parser_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_RECEIVER_RTP_RECEIVER_H_
