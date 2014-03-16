// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_TRANSPORT_VIDEO_SENDER_H_
#define MEDIA_CAST_TRANSPORT_TRANSPORT_VIDEO_SENDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/rtp_sender/rtp_sender.h"
#include "media/cast/transport/utility/transport_encryption_handler.h"

namespace media {
class VideoFrame;

namespace cast {
class LoggingImpl;

namespace transport {
class PacedSender;

// Not thread safe. Only called from the main cast transport thread.
// This class owns all objects related to sending coded video, objects that
// encrypt, create RTP packets and send to network.
class TransportVideoSender : public base::NonThreadSafe {
 public:
  TransportVideoSender(
      const CastTransportVideoConfig& config,
      base::TickClock* clock,
      LoggingImpl* logging,
      const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner,
      PacedSender* const paced_packet_sender);

  virtual ~TransportVideoSender();

  // Handles the encoded video frames to be processed.
  // Frames will be encrypted, packetized and transmitted to the network.
  void InsertCodedVideoFrame(const EncodedVideoFrame* coded_frame,
                             const base::TimeTicks& capture_time);

  // Retransmision request.
  void ResendPackets(
      const MissingFramesAndPacketsMap& missing_frames_and_packets);

  bool initialized() const { return initialized_; }

  // Subscribe callback to get RTP Audio stats.
  void SubscribeVideoRtpStatsCallback(
      const CastTransportRtpStatistics& callback);

 private:
  // Caller must allocate the destination |encrypted_video_frame| the data
  // member will be resized to hold the encrypted size.
  bool EncryptVideoFrame(const EncodedVideoFrame& encoded_frame,
                         EncodedVideoFrame* encrypted_video_frame);

  const base::TimeDelta rtp_max_delay_;
  TransportEncryptionHandler encryptor_;
  RtpSender rtp_sender_;
  bool initialized_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TransportVideoSender);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_TRANSPORT_VIDEO_SENDER_H_
