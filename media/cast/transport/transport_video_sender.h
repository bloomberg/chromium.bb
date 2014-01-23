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

namespace crypto {
class Encryptor;
}

namespace media {
class VideoFrame;

namespace cast {
namespace transport {

class PacedSender;

// Not thread safe. Only called from the main cast transport thread.
// This class owns all objects related to sending coded video, objects that
// encrypt, create RTP packets and send to network.
class TransportVideoSender : public base::NonThreadSafe {
 public:
  TransportVideoSender(const CastTransportConfig& config,
                       base::TickClock* clock,
                       PacedSender* const paced_packet_sender);

  virtual ~TransportVideoSender();

  // Handles the encoded video frames to be processed.
  // Frames will be encrypted, packetized and transmitted to the network.
  void InsertCodedVideoFrame(const EncodedVideoFrame* coded_frame,
                             const base::TimeTicks& capture_time);

  // Retrieves video RTP statistics.
  void GetStatistics(const base::TimeTicks& now, RtcpSenderInfo* sender_info);

  // Retransmision request.
  void ResendPackets(
      const MissingFramesAndPacketsMap& missing_frames_and_packets);

 private:
  // Caller must allocate the destination |encrypted_video_frame| the data
  // member will be resized to hold the encrypted size.
  bool EncryptVideoFrame(const EncodedVideoFrame& encoded_frame,
                         EncodedVideoFrame* encrypted_video_frame);

  const base::TimeDelta rtp_max_delay_;

  RtpSender rtp_sender_;
  scoped_ptr<crypto::Encryptor> encryptor_;
  std::string iv_mask_;

  bool initialized_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TransportVideoSender);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_TRANSPORT_VIDEO_SENDER_H_
