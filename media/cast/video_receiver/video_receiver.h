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
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/cast_thread.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_common/rtp_defines.h"
#include "media/cast/rtp_receiver/rtp_receiver.h"

namespace media {
namespace cast {

class Framer;
class LocalRtpVideoData;
class LocalRtpVideoFeedback;
class PacedPacketSender;
class PeerVideoReceiver;
class Rtcp;
class RtpReceiverStatistics;
class VideoDecoder;


// Should only be called from the Main cast thread.
class VideoReceiver : public base::NonThreadSafe,
                      public base::SupportsWeakPtr<VideoReceiver> {
 public:

  VideoReceiver(scoped_refptr<CastThread> cast_thread,
                const VideoReceiverConfig& video_config,
                PacedPacketSender* const packet_sender);

  virtual ~VideoReceiver();

  // Request a raw frame. Will return frame via callback when available.
  void GetRawVideoFrame(const VideoFrameDecodedCallback& callback);

  // Request an encoded frame. Memory allocated by application.
  bool GetEncodedVideoFrame(EncodedVideoFrame* video_frame,
                            base::TimeTicks* render_time);

  // Insert a RTP packet to the video receiver.
  void IncomingPacket(const uint8* packet, int length);

  // Release frame - should be called following a GetEncodedVideoFrame call.
  // Removes frame from the frame map in the framer.
  void ReleaseFrame(uint8 frame_id);

  void set_clock(base::TickClock* clock) {
    clock_ = clock;
    rtcp_->set_clock(clock);
  }
 protected:
  void IncomingRtpPacket(const uint8* payload_data,
                         int payload_size,
                         const RtpCastHeader& rtp_header);

  void DecodeVideoFrameThread(
      const EncodedVideoFrame* encoded_frame,
      const base::TimeTicks render_time,
      const VideoFrameDecodedCallback& frame_decoded_callback,
      base::Closure frame_release_callback);

 private:
  friend class LocalRtpVideoData;
  friend class LocalRtpVideoFeedback;

  void CastFeedback(const RtcpCastMessage& cast_message);
  void RequestKeyFrame();

  // Returns Render time based on current time and the rtp timestamp.
  base::TimeTicks GetRenderTime(base::TimeTicks now, uint32 rtp_timestamp);

  // Schedule timing for the next cast message.
  void ScheduleNextCastMessage();

  // Schedule timing for the next RTCP report.
  void ScheduleNextRtcpReport();
  // Actually send the next cast message.
  void SendNextCastMessage();
  // Actually send the next RTCP report.
  void SendNextRtcpReport();

  scoped_refptr<VideoDecoder> video_decoder_;
  scoped_refptr<CastThread> cast_thread_;
  scoped_ptr<Framer> framer_;
  const VideoCodec codec_;
  const uint32 incoming_ssrc_;
  base::TimeDelta target_delay_delta_;
  scoped_ptr<LocalRtpVideoData> incoming_payload_callback_;
  scoped_ptr<LocalRtpVideoFeedback> incoming_payload_feedback_;
  RtpReceiver rtp_receiver_;
  scoped_ptr<Rtcp> rtcp_;
  scoped_ptr<RtpReceiverStatistics> rtp_video_receiver_statistics_;
  base::TimeTicks time_last_sent_cast_message_;
  // Sender-receiver offset estimation.
  base::TimeDelta time_offset_;

  scoped_ptr<base::TickClock> default_tick_clock_;
  base::TickClock* clock_;

  base::WeakPtrFactory<VideoReceiver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoReceiver);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_RECEIVER_VIDEO_RECEIVER_H_

