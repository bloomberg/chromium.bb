// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_VIDEO_RECEIVER_VIDEO_RECEIVER_H_
#define MEDIA_CAST_VIDEO_RECEIVER_VIDEO_RECEIVER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_receiver/rtp_receiver.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"
#include "media/cast/transport/utility/transport_encryption_handler.h"

namespace media {
namespace cast {

class Framer;
class LocalRtpVideoData;
class LocalRtpVideoFeedback;
class PeerVideoReceiver;
class Rtcp;
class RtpReceiverStatistics;
class VideoDecoder;

// Callback used by the video receiver to inform the audio receiver of the new
// delay used to compute the playout and render times.
typedef base::Callback<void(base::TimeDelta)> SetTargetDelayCallback;

// Should only be called from the Main cast thread.
class VideoReceiver : public base::NonThreadSafe,
                      public base::SupportsWeakPtr<VideoReceiver> {
 public:
  VideoReceiver(scoped_refptr<CastEnvironment> cast_environment,
                const VideoReceiverConfig& video_config,
                transport::PacedPacketSender* const packet_sender,
                const SetTargetDelayCallback& target_delay_cb);

  virtual ~VideoReceiver();

  // Request a raw frame. Will return frame via callback when available.
  void GetRawVideoFrame(const VideoFrameDecodedCallback& callback);

  // Request an encoded frame. Will return frame via callback when available.
  void GetEncodedVideoFrame(const VideoFrameEncodedCallback& callback);

  // Insert a RTP packet to the video receiver.
  void IncomingPacket(scoped_ptr<Packet> packet);

 protected:
  void IncomingParsedRtpPacket(const uint8* payload_data,
                               size_t payload_size,
                               const RtpCastHeader& rtp_header);

  void DecodeVideoFrameThread(
      scoped_ptr<transport::EncodedVideoFrame> encoded_frame,
      const base::TimeTicks render_time,
      const VideoFrameDecodedCallback& frame_decoded_callback);

 private:
  friend class LocalRtpVideoData;
  friend class LocalRtpVideoFeedback;

  void CastFeedback(const RtcpCastMessage& cast_message);

  void DecodeVideoFrame(const VideoFrameDecodedCallback& callback,
                        scoped_ptr<transport::EncodedVideoFrame> encoded_frame,
                        const base::TimeTicks& render_time);

  bool DecryptVideoFrame(scoped_ptr<transport::EncodedVideoFrame>* video_frame);

  bool PullEncodedVideoFrame(
      bool next_frame,
      scoped_ptr<transport::EncodedVideoFrame>* encoded_frame,
      base::TimeTicks* render_time);

  void PlayoutTimeout();

  // Returns Render time based on current time and the rtp timestamp.
  base::TimeTicks GetRenderTime(base::TimeTicks now, uint32 rtp_timestamp);

  void InitializeTimers();

  // Schedule timing for the next cast message.
  void ScheduleNextCastMessage();

  // Schedule timing for the next RTCP report.
  void ScheduleNextRtcpReport();

  // Actually send the next cast message.
  void SendNextCastMessage();

  // Actually send the next RTCP report.
  void SendNextRtcpReport();

  // Update the target delay based on past information. Will also update the
  // rtcp module and the audio receiver.
  void UpdateTargetDelay();

  scoped_ptr<VideoDecoder> video_decoder_;
  scoped_refptr<CastEnvironment> cast_environment_;

  // Subscribes to raw events.
  // Processes raw audio events to be sent over to the cast sender via RTCP.
  ReceiverRtcpEventSubscriber event_subscriber_;

  scoped_ptr<Framer> framer_;
  const transport::VideoCodec codec_;
  base::TimeDelta target_delay_delta_;
  base::TimeDelta frame_delay_;
  scoped_ptr<LocalRtpVideoData> incoming_payload_callback_;
  scoped_ptr<LocalRtpVideoFeedback> incoming_payload_feedback_;
  RtpReceiver rtp_receiver_;
  scoped_ptr<Rtcp> rtcp_;
  scoped_ptr<RtpReceiverStatistics> rtp_video_receiver_statistics_;
  base::TimeDelta time_offset_;  // Sender-receiver offset estimation.
  int time_offset_counter_;
  transport::TransportEncryptionHandler decryptor_;
  std::list<VideoFrameEncodedCallback> queued_encoded_callbacks_;
  bool time_incoming_packet_updated_;
  base::TimeTicks time_incoming_packet_;
  uint32 incoming_rtp_timestamp_;
  base::TimeTicks last_render_time_;
  SetTargetDelayCallback target_delay_cb_;

  // This mapping allows us to log kVideoAckSent as a frame event. In addition
  // it allows the event to be transmitted via RTCP.
  RtpTimestamp frame_id_to_rtp_timestamp_[256];

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoReceiver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoReceiver);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_RECEIVER_VIDEO_RECEIVER_H_
