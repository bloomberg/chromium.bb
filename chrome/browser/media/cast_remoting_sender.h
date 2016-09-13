// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_REMOTING_SENDER_H_
#define CHROME_BROWSER_MEDIA_CAST_REMOTING_SENDER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/rtcp/rtcp_defines.h"

namespace cast {

// RTP sender for a single Cast Remoting RTP stream. The client calls Send() to
// instruct the sender to transmit the data using a CastTransport.
//
// This class is instantiated and owned by CastTransportHostFilter in response
// to IPC messages from an extension process to create RTP streams for the media
// remoting use case. CastTransportHostFilter is also responsible for destroying
// the instance in response to later IPCs.
//
// TODO(miu): Mojo service bindings/implementation to read from data pipes will
// be added in a soon-upcoming change.

class CastRemotingSender {
 public:
  // |transport| is expected to outlive this class.
  explicit CastRemotingSender(
      scoped_refptr<media::cast::CastEnvironment> cast_environment,
      media::cast::CastTransport* transport,
      const media::cast::CastTransportRtpConfig& config);
  ~CastRemotingSender();

 private:
  // Friend class for unit tests.
  friend class CastRemotingSenderTest;

  class RemotingRtcpClient;

  // Called to send the serialized frame data.
  void SendFrame();

  // Called to cancel all the in flight frames when seeking happens.
  void CancelFramesInFlight();

  // These are called to deliver RTCP feedback from the receiver.
  void OnReceivedCastMessage(const media::cast::RtcpCastMessage& cast_feedback);
  void OnReceivedRtt(base::TimeDelta round_trip_time);

  // Returns the number of frames that were sent but not yet acknowledged. This
  // does not account for frames acknowledged out-of-order, and is always a high
  // watermark estimate.
  int NumberOfFramesInFlight() const;

  // Schedule and execute periodic checks for re-sending packets.  If no
  // acknowledgements have been received for "too long," CastRemotingSender will
  // speculatively re-send certain packets of an unacked frame to kick-start
  // re-transmission.  This is a last resort tactic to prevent the session from
  // getting stuck after a long outage.
  void ScheduleNextResendCheck();
  void ResendCheck();
  void ResendForKickstart();

  void RecordLatestFrameTimestamps(media::cast::FrameId frame_id,
                                   media::cast::RtpTimeTicks rtp_timestamp);
  media::cast::RtpTimeTicks GetRecordedRtpTimestamp(
      media::cast::FrameId frame_id) const;

  // Unique identifier for the RTP stream and this CastRemotingSender.
  const int32_t remoting_stream_id_;

  const scoped_refptr<media::cast::CastEnvironment> cast_environment_;

  // Sends encoded frames over the configured transport (e.g., UDP). It outlives
  // this class.
  media::cast::CastTransport* const transport_;

  const uint32_t ssrc_;

  const bool is_audio_;

  // This is the maximum delay that the sender should get ack from receiver.
  // Otherwise, sender will call ResendForKickstart().
  base::TimeDelta max_ack_delay_;

  // This is "null" until the first frame is sent.  Thereafter, this tracks the
  // last time any frame was sent or re-sent.
  base::TimeTicks last_send_time_;

  // The ID of the last frame sent.  This member is invalid until
  // |!last_send_time_.is_null()|.
  media::cast::FrameId last_sent_frame_id_;

  // The ID of the latest (not necessarily the last) frame that has been
  // acknowledged.  This member is invalid until |!last_send_time_.is_null()|.
  media::cast::FrameId latest_acked_frame_id_;

  // Counts the number of duplicate ACK that are being received.  When this
  // number reaches a threshold, the sender will take this as a sign that the
  // receiver hasn't yet received the first packet of the next frame.  In this
  // case, CastRemotingSender will trigger a re-send of the next frame.
  int duplicate_ack_counter_;

  // The most recently measured round trip time.
  base::TimeDelta current_round_trip_time_;

  // The next frame's payload data. Populated by one or more calls to
  // ConsumeDataChunk().
  // TODO(miu): To be implemented in soon upcoming change.
  std::string next_frame_data_;

  // Ring buffer to keep track of recent frame RTP timestamps. This should
  // only be accessed through the Record/GetXX() methods. The index into this
  // ring buffer is the lower 8 bits of the FrameId.
  media::cast::RtpTimeTicks frame_rtp_timestamps_[256];

  // This flag indicates whether CancelFramesInFlight() was called.
  bool last_frame_was_canceled_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<CastRemotingSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastRemotingSender);
};

}  // namespace cast

#endif  // CHROME_BROWSER_MEDIA_CAST_REMOTING_SENDER_H_
