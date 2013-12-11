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
#include "media/cast/net/rtp_sender/rtp_sender.h"
#include "media/cast/rtcp/rtcp.h"

namespace crypto {
  class Encryptor;
}

namespace media {
class VideoFrame;
}

namespace media {
namespace cast {

class VideoEncoder;
class LocalRtcpVideoSenderFeedback;
class LocalRtpVideoSenderStatistics;
class LocalVideoEncoderCallback;
class PacedPacketSender;

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
              VideoEncoderController* const video_encoder_controller,
              PacedPacketSender* const paced_packet_sender);

  virtual ~VideoSender();

  // The video_frame must be valid until the closure callback is called.
  // The closure callback is called from the video encoder thread as soon as
  // the encoder is done with the frame; it does not mean that the encoded frame
  // has been sent out.
  void InsertRawVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& capture_time);

  // The video_frame must be valid until the closure callback is called.
  // The closure callback is called from the main thread as soon as
  // the cast sender is done with the frame; it does not mean that the encoded
  // frame has been sent out.
  void InsertCodedVideoFrame(const EncodedVideoFrame* video_frame,
                             const base::TimeTicks& capture_time,
                             const base::Closure callback);

  // Only called from the main cast thread.
  void IncomingRtcpPacket(const uint8* packet, size_t length,
                          const base::Closure callback);

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

  void SendEncodedVideoFrame(const EncodedVideoFrame* video_frame,
                             const base::TimeTicks& capture_time);
  void ResendFrame(uint32 resend_frame_id);
  void ReceivedAck(uint32 acked_frame_id);
  void UpdateFramesInFlight();

  void SendEncodedVideoFrameMainThread(
      scoped_ptr<EncodedVideoFrame> video_frame,
      const base::TimeTicks& capture_time);

  void InitializeTimers();

  // Caller must allocate the destination |encrypted_video_frame| the data
  // member will be resized to hold the encrypted size.
  bool EncryptVideoFrame(const EncodedVideoFrame& encoded_frame,
                         EncodedVideoFrame* encrypted_video_frame);

  const base::TimeDelta rtp_max_delay_;
  const int max_frame_rate_;

  scoped_refptr<CastEnvironment> cast_environment_;
  scoped_ptr<LocalRtcpVideoSenderFeedback> rtcp_feedback_;
  scoped_ptr<LocalRtpVideoSenderStatistics> rtp_video_sender_statistics_;
  scoped_ptr<VideoEncoder> video_encoder_;
  scoped_ptr<Rtcp> rtcp_;
  scoped_ptr<RtpSender> rtp_sender_;
  VideoEncoderController* video_encoder_controller_;
  uint8 max_unacked_frames_;
  scoped_ptr<crypto::Encryptor> encryptor_;
  std::string iv_mask_;
  int last_acked_frame_id_;
  int last_sent_frame_id_;
  int duplicate_ack_;
  base::TimeTicks last_send_time_;
  base::TimeTicks last_checked_skip_count_time_;
  int last_skip_count_;
  CongestionControl congestion_control_;

  bool initialized_;
  base::WeakPtrFactory<VideoSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_SENDER_VIDEO_SENDER_H_

