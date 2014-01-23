// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_TRANSPORT_AUDIO_SENDER_H_
#define MEDIA_CAST_TRANSPORT_TRANSPORT_AUDIO_SENDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "media/cast/transport/rtp_sender/rtp_sender.h"

namespace crypto {
class Encryptor;
}

namespace media {
namespace cast {
namespace transport {

class PacedSender;

// It's only called from the main cast transport thread.
class TransportAudioSender : public base::NonThreadSafe {
 public:
  TransportAudioSender(const CastTransportConfig& config,
                       base::TickClock* clock,
                       PacedSender* const paced_packet_sender);

  virtual ~TransportAudioSender();

  // Handles the encoded audio frames to be processed.
  // Frames will be encrypted, packetized and transmitted to the network.
  void InsertCodedAudioFrame(const EncodedAudioFrame* audio_frame,
                             const base::TimeTicks& recorded_time);

  // Retrieves audio RTP statistics.
  void GetStatistics(const base::TimeTicks& now, RtcpSenderInfo* sender_info);

  // Retransmision request.
  void ResendPackets(
      const MissingFramesAndPacketsMap& missing_frames_and_packets);

 private:
  friend class LocalRtcpAudioSenderFeedback;

  // Caller must allocate the destination |encrypted_frame|. The data member
  // will be resized to hold the encrypted size.
  bool EncryptAudioFrame(const EncodedAudioFrame& audio_frame,
                         EncodedAudioFrame* encrypted_frame);

  RtpSender rtp_sender_;
  bool initialized_;
  scoped_ptr<crypto::Encryptor> encryptor_;
  std::string iv_mask_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TransportAudioSender);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_TRANSPORT_AUDIO_SENDER_H_
