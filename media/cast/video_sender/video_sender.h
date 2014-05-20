// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_VIDEO_SENDER_VIDEO_SENDER_H_
#define MEDIA_CAST_VIDEO_SENDER_VIDEO_SENDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/congestion_control/congestion_control.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_timestamp_helper.h"

namespace media {
class VideoFrame;

namespace cast {
class LocalRtcpVideoSenderFeedback;
class LocalVideoEncoderCallback;
class VideoEncoder;

namespace transport {
class CastTransportSender;
}

// Not thread safe. Only called from the main cast thread.
// This class owns all objects related to sending video, objects that create RTP
// packets, congestion control, video encoder, parsing and sending of
// RTCP packets.
// Additionally it posts a bunch of delayed tasks to the main thread for various
// timeouts.
class VideoSender : public base::NonThreadSafe,
                    public base::SupportsWeakPtr<VideoSender> {
 public:
  VideoSender(scoped_refptr<CastEnvironment> cast_environment,
              const VideoSenderConfig& video_config,
              const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
              const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb,
              const CastInitializationCallback& cast_initialization_cb,
              transport::CastTransportSender* const transport_sender);

  virtual ~VideoSender();

  // The video_frame must be valid until the closure callback is called.
  // The closure callback is called from the video encoder thread as soon as
  // the encoder is done with the frame; it does not mean that the encoded frame
  // has been sent out.
  void InsertRawVideoFrame(const scoped_refptr<media::VideoFrame>& video_frame,
                           const base::TimeTicks& capture_time);

  // Only called from the main cast thread.
  void IncomingRtcpPacket(scoped_ptr<Packet> packet);

 protected:
  // Protected for testability.
  void OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback);

 private:
  friend class LocalRtcpVideoSenderFeedback;

  // Schedule when we should send the next RTPC report,
  // via a PostDelayedTask to the main cast thread.
  void ScheduleNextRtcpReport();
  void SendRtcpReport();

  // Schedule when we should check that we have received an acknowledgment, or a
  // loss report from our remote peer. If we have not heard back from our remote
  // peer we speculatively resend our oldest unacknowledged frame (the whole
  // frame). Note for this to happen we need to lose all pending packets (in
  // normal operation 3 full frames), hence this is the last resort to prevent
  // us getting stuck after a long outage.
  void ScheduleNextResendCheck();
  void ResendCheck();

  // Monitor how many frames that are silently dropped by the video sender
  // per time unit.
  void ScheduleNextSkippedFramesCheck();
  void SkippedFramesCheck();

  void ResendFrame(uint32 resend_frame_id);
  void ReceivedAck(uint32 acked_frame_id);
  void UpdateFramesInFlight();

  void SendEncodedVideoFrameMainThread(
      int requested_bitrate_before_encode,
      scoped_ptr<transport::EncodedFrame> encoded_frame);

  void InitializeTimers();

  void UpdateBitrate(int32 new_bitrate);

  base::TimeDelta rtp_max_delay_;
  const int max_frame_rate_;

  scoped_refptr<CastEnvironment> cast_environment_;
  transport::CastTransportSender* const transport_sender_;

  RtpTimestampHelper rtp_timestamp_helper_;
  scoped_ptr<LocalRtcpVideoSenderFeedback> rtcp_feedback_;
  scoped_ptr<VideoEncoder> video_encoder_;
  scoped_ptr<Rtcp> rtcp_;
  uint8 max_unacked_frames_;
  int last_acked_frame_id_;
  int last_sent_frame_id_;
  int frames_in_encoder_;
  int duplicate_ack_;
  base::TimeTicks last_send_time_;
  base::TimeTicks last_checked_skip_count_time_;
  int last_skip_count_;
  int current_requested_bitrate_;
  // When we get close to the max number of un-acked frames, we set lower
  // the bitrate drastically to ensure that we catch up. Without this we
  // risk getting stuck in a catch-up state forever.
  int current_bitrate_divider_;
  CongestionControl congestion_control_;

  // This is a "good enough" mapping for finding the RTP timestamp associated
  // with a video frame. The key is the lowest 8 bits of frame id (which is
  // what is sent via RTCP). This map is used for logging purposes. The only
  // time when this mapping will be incorrect is when it receives an ACK for a
  // old enough frame such that 8-bit wrap around has already occurred, which
  // should be pretty rare.
  RtpTimestamp frame_id_to_rtp_timestamp_[256];

  bool initialized_;
  // Indicator for receiver acknowledgments.
  bool active_session_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_SENDER_VIDEO_SENDER_H_
