// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the base class for an object that send frames to a receiver.
// TODO(hclam): Refactor such that there is no separate AudioSender vs.
// VideoSender, and the functionality of both is rolled into this class.

#ifndef MEDIA_CAST_SENDER_FRAME_SENDER_H_
#define MEDIA_CAST_SENDER_FRAME_SENDER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/rtcp/rtcp.h"
#include "media/cast/sender/congestion_control.h"

namespace media {
namespace cast {

class FrameSender {
 public:
  FrameSender(scoped_refptr<CastEnvironment> cast_environment,
              bool is_audio,
              CastTransportSender* const transport_sender,
              base::TimeDelta rtcp_interval,
              int rtp_timebase,
              uint32 ssrc,
              double max_frame_rate,
              base::TimeDelta playout_delay,
              CongestionControl* congestion_control);
  virtual ~FrameSender();

  // Calling this function is only valid if the receiver supports the
  // "extra_playout_delay", rtp extension.
  void SetTargetPlayoutDelay(base::TimeDelta new_target_playout_delay);

  base::TimeDelta GetTargetPlayoutDelay() const {
    return target_playout_delay_;
  }

  // Called by the encoder with the next EncodeFrame to send.
  void SendEncodedFrame(int requested_bitrate_before_encode,
                        scoped_ptr<EncodedFrame> encoded_frame);

 protected:
  // Schedule and execute periodic sending of RTCP report.
  void ScheduleNextRtcpReport();
  void SendRtcpReport(bool schedule_future_reports);

  void OnReceivedRtt(base::TimeDelta rtt,
                     base::TimeDelta avg_rtt,
                     base::TimeDelta min_rtt,
                     base::TimeDelta max_rtt);

  bool is_rtt_available() const { return rtt_available_; }

  const scoped_refptr<CastEnvironment> cast_environment_;

  // Sends encoded frames over the configured transport (e.g., UDP).  In
  // Chromium, this could be a proxy that first sends the frames from a renderer
  // process to the browser process over IPC, with the browser process being
  // responsible for "packetizing" the frames and pushing packets into the
  // network layer.
  CastTransportSender* const transport_sender_;

  const uint32 ssrc_;

  // RTT information from RTCP.
  bool rtt_available_;
  base::TimeDelta rtt_;
  base::TimeDelta avg_rtt_;
  base::TimeDelta min_rtt_;
  base::TimeDelta max_rtt_;

 protected:
  // Schedule and execute periodic checks for re-sending packets.  If no
  // acknowledgements have been received for "too long," AudioSender will
  // speculatively re-send certain packets of an unacked frame to kick-start
  // re-transmission.  This is a last resort tactic to prevent the session from
  // getting stuck after a long outage.
  void ScheduleNextResendCheck();
  void ResendCheck();
  void ResendForKickstart();

  // Protected for testability.
  void OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback);

  // Returns true if there are too many frames in flight, or if the media
  // duration of the frames in flight would be too high by sending the next
  // frame.  The latter metric is determined from the given |capture_time|
  // for the next frame to be encoded and sent.
  bool ShouldDropNextFrame(base::TimeTicks capture_time) const;

  // Record or retrieve a recent history of each frame's timestamps.
  // Warning: If a frame ID too far in the past is requested, the getters will
  // silently succeed but return incorrect values.  Be sure to respect
  // media::cast::kMaxUnackedFrames.
  void RecordLatestFrameTimestamps(uint32 frame_id,
                                   base::TimeTicks reference_time,
                                   RtpTimestamp rtp_timestamp);
  base::TimeTicks GetRecordedReferenceTime(uint32 frame_id) const;
  RtpTimestamp GetRecordedRtpTimestamp(uint32 frame_id) const;

  // Called when we get an ACK for a frame.
  virtual void OnAck(uint32 frame_id) = 0;

  const base::TimeDelta rtcp_interval_;

  // The total amount of time between a frame's capture/recording on the sender
  // and its playback on the receiver (i.e., shown to a user).  This is fixed as
  // a value large enough to give the system sufficient time to encode,
  // transmit/retransmit, receive, decode, and render; given its run-time
  // environment (sender/receiver hardware performance, network conditions,
  // etc.).
  base::TimeDelta target_playout_delay_;

  // If true, we transmit the target playout delay to the receiver.
  bool send_target_playout_delay_;

  // Max encoded frames generated per second.
  double max_frame_rate_;

  // Maximum number of outstanding frames before the encoding and sending of
  // new frames shall halt.
  int max_unacked_frames_;

  // The number of frames currently being processed in |video_encoder_|.
  int frames_in_encoder_;

  // Counts how many RTCP reports are being "aggressively" sent (i.e., one per
  // frame) at the start of the session.  Once a threshold is reached, RTCP
  // reports are instead sent at the configured interval + random drift.
  int num_aggressive_rtcp_reports_sent_;

  // This is "null" until the first frame is sent.  Thereafter, this tracks the
  // last time any frame was sent or re-sent.
  base::TimeTicks last_send_time_;

  // The ID of the last frame sent.  Logic throughout FrameSender assumes this
  // can safely wrap-around.  This member is invalid until
  // |!last_send_time_.is_null()|.
  uint32 last_sent_frame_id_;

  // The ID of the latest (not necessarily the last) frame that has been
  // acknowledged.  Logic throughout AudioSender assumes this can safely
  // wrap-around.  This member is invalid until |!last_send_time_.is_null()|.
  uint32 latest_acked_frame_id_;

  // Counts the number of duplicate ACK that are being received.  When this
  // number reaches a threshold, the sender will take this as a sign that the
  // receiver hasn't yet received the first packet of the next frame.  In this
  // case, VideoSender will trigger a re-send of the next frame.
  int duplicate_ack_counter_;

  // If this sender is ready for use, this is STATUS_AUDIO_INITIALIZED or
  // STATUS_VIDEO_INITIALIZED.
  CastInitializationStatus cast_initialization_status_;

  // RTP timestamp increment representing one second.
  const int rtp_timebase_;

  // This object controls how we change the bitrate to make sure the
  // buffer doesn't overflow.
  scoped_ptr<CongestionControl> congestion_control_;

 private:
  const bool is_audio_;

  // Ring buffers to keep track of recent frame timestamps (both in terms of
  // local reference time and RTP media time).  These should only be accessed
  // through the Record/GetXXX() methods.
  base::TimeTicks frame_reference_times_[256];
  RtpTimestamp frame_rtp_timestamps_[256];

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FrameSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_SENDER_FRAME_SENDER_H_
