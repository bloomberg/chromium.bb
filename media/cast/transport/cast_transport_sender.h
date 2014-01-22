// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the main interface for the cast transport sender. The cast sender
// handles the cast pipeline from encoded frames (both audio and video), to
// encryption, packetization and transport.
// All configurations are done at creation.

#ifndef MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_SENDER_H_
#define MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_SENDER_H_

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
class AudioBus;
class VideoFrame;
}

namespace media {
namespace cast {
namespace transport {

typedef base::Callback<void(CastTransportStatus status)>
    CastTransportStatusCallback;

// This Class is not thread safe.
// The application should only trigger this class from the transport thread.
class CastTransportSender : public base::NonThreadSafe {
 public:
  static CastTransportSender* CreateCastNetSender(
      base::TickClock* clock,
      const CastTransportConfig& config,
      const CastTransportStatusCallback& status_callback,
      scoped_refptr<base::TaskRunner> transport_task_runner);

  virtual ~CastTransportSender() {}

  // Sets the Cast packet receiver. Should be called after creation on the
  // Cast sender.
  virtual void SetPacketReceiver(
      scoped_refptr<PacketReceiver> packet_receiver) = 0;

  // The following two functions handle the encoded media frames (audio and
  // video) to be processed.
  // Frames will be encrypted, packetized and transmitted to the network.
  virtual void InsertCodedAudioFrame(const EncodedAudioFrame* audio_frame,
                                     const base::TimeTicks& recorded_time) = 0;

  virtual void InsertCodedVideoFrame(const EncodedVideoFrame* video_frame,
                                     const base::TimeTicks& capture_time) = 0;

  // Builds an RTCP packet and sends it to the network.
  virtual void SendRtcpFromRtpSender(
      uint32 packet_type_flags,
      const RtcpSenderInfo& sender_info,
      const RtcpDlrrReportBlock& dlrr,
      const RtcpSenderLogMessage& sender_log) = 0;

  // Retransmission request.
  virtual void ResendPackets(
      const MissingFramesAndPacketsMap& missing_packets) = 0;

  // Retrieves audio RTP statistics.
  virtual void RtpAudioStatistics(const base::TimeTicks& now,
                                  RtcpSenderInfo* sender_info) = 0;

  // Retrieves audio RTP statistics.
  virtual void RtpVideoStatistics(const base::TimeTicks& now,
                                  RtcpSenderInfo* sender_info) = 0;
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_SENDER_H_
