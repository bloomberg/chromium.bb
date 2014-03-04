// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the interface to the cast RTP sender.

#ifndef MEDIA_CAST_TRANSPORT_RTP_SENDER_RTP_SENDER_H_
#define MEDIA_CAST_TRANSPORT_RTP_SENDER_RTP_SENDER_H_

#include <map>
#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/memory/weak_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "media/cast/transport/cast_transport_sender.h"
#include "media/cast/transport/pacing/paced_sender.h"
#include "media/cast/transport/rtp_sender/packet_storage/packet_storage.h"
#include "media/cast/transport/rtp_sender/rtp_packetizer/rtp_packetizer.h"

namespace media {
namespace cast {
namespace transport {

// This object is only called from the main cast thread.
// This class handles splitting encoded audio and video frames into packets and
// add an RTP header to each packet. The sent packets are stored until they are
// acknowledged by the remote peer or timed out.
class RtpSender {
 public:
  RtpSender(
      base::TickClock* clock,
      const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner,
      PacedSender* const transport);

  ~RtpSender();

  // Initialize audio stack. Audio must be initialized prior to sending encoded
  // audio frames.
  void InitializeAudio(const CastTransportAudioConfig& config);

  // Initialize video stack. Video must be initialized prior to sending encoded
  // video frames.
  void InitializeVideo(const CastTransportVideoConfig& config);

  // The video_frame objects ownership is handled by the main cast thread.
  void IncomingEncodedVideoFrame(const EncodedVideoFrame* video_frame,
                                 const base::TimeTicks& capture_time);

  // The audio_frame objects ownership is handled by the main cast thread.
  void IncomingEncodedAudioFrame(const EncodedAudioFrame* audio_frame,
                                 const base::TimeTicks& recorded_time);

  void ResendPackets(const MissingFramesAndPacketsMap& missing_packets);

  // Set the callback on which RTP statistics data will be returned. Calling
  // this function would start a timer that would schedule the callback in
  // a constant interval.
  void SubscribeRtpStatsCallback(const CastTransportRtpStatistics& callback);

 private:
  void ScheduleNextStatsReport();
  void RtpStatistics();
  void UpdateSequenceNumber(Packet* packet);

  base::TickClock* clock_;  // Not owned by this class.
  RtpPacketizerConfig config_;
  scoped_ptr<RtpPacketizer> packetizer_;
  scoped_ptr<PacketStorage> storage_;
  PacedSender* const transport_;
  CastTransportRtpStatistics stats_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> transport_task_runner_;

  base::WeakPtrFactory<RtpSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RtpSender);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_RTP_SENDER_RTP_SENDER_H_
