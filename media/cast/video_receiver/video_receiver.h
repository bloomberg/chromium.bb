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
#include "media/cast/base/clock_drift_smoother.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/framer/framer.h"
#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/rtp_receiver/rtp_receiver.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"
#include "media/cast/transport/utility/transport_encryption_handler.h"

namespace media {

class VideoFrame;

namespace cast {

class VideoDecoder;

// VideoReceiver receives packets out-of-order while clients make requests for
// complete frames in-order.  (A frame consists of one or more packets.)
//
// VideoReceiver also includes logic for computing the playout time for each
// frame, accounting for a constant targeted playout delay.  The purpose of the
// playout delay is to provide a fixed window of time between the capture event
// on the sender and the playout on the receiver.  This is important because
// each step of the pipeline (i.e., encode frame, then transmit/retransmit from
// the sender, then receive and re-order packets on the receiver, then decode
// frame) can vary in duration and is typically very hard to predict.
//
// Two types of frames can be requested: 1) A frame of decoded video data; or 2)
// a frame of still-encoded video data, to be passed into an external video
// decoder.  Each request for a frame includes a callback which VideoReceiver
// guarantees will be called at some point in the future unless the
// VideoReceiver is destroyed. Clients should generally limit the number of
// outstanding requests (perhaps to just one or two).
//
// This class is not thread safe.  Should only be called from the Main cast
// thread.
class VideoReceiver : public RtpReceiver,
                      public RtpPayloadFeedback,
                      public base::NonThreadSafe,
                      public base::SupportsWeakPtr<VideoReceiver> {
 public:
  VideoReceiver(scoped_refptr<CastEnvironment> cast_environment,
                const VideoReceiverConfig& video_config,
                transport::PacedPacketSender* const packet_sender);

  virtual ~VideoReceiver();

  // Request a decoded video frame.
  //
  // The given |callback| is guaranteed to be run at some point in the future,
  // even if to respond with NULL at shutdown time.
  void GetRawVideoFrame(const VideoFrameDecodedCallback& callback);

  // Request an encoded video frame.
  //
  // The given |callback| is guaranteed to be run at some point in the future,
  // even if to respond with NULL at shutdown time.
  void GetEncodedVideoFrame(const FrameEncodedCallback& callback);

  // Deliver another packet, possibly a duplicate, and possibly out-of-order.
  void IncomingPacket(scoped_ptr<Packet> packet);

 protected:
  friend class VideoReceiverTest;  // Invokes OnReceivedPayloadData().

  virtual void OnReceivedPayloadData(const uint8* payload_data,
                                     size_t payload_size,
                                     const RtpCastHeader& rtp_header) OVERRIDE;

  // RtpPayloadFeedback implementation.
  virtual void CastFeedback(const RtcpCastMessage& cast_message) OVERRIDE;

 private:
  // Processes ready-to-consume packets from |framer_|, decrypting each packet's
  // payload data, and then running the enqueued callbacks in order (one for
  // each packet).  This method may post a delayed task to re-invoke itself in
  // the future to wait for missing/incomplete frames.
  void EmitAvailableEncodedFrames();

  // Clears the |is_waiting_for_consecutive_frame_| flag and invokes
  // EmitAvailableEncodedFrames().
  void EmitAvailableEncodedFramesAfterWaiting();

  // Feeds an EncodedFrame into |video_decoder_|.  GetRawVideoFrame() uses this
  // as a callback for GetEncodedVideoFrame().
  void DecodeEncodedVideoFrame(
      const VideoFrameDecodedCallback& callback,
      scoped_ptr<transport::EncodedFrame> encoded_frame);

  // Computes the playout time for a frame with the given |rtp_timestamp|.
  // Because lip-sync info is refreshed regularly, calling this method with the
  // same argument may return different results.
  base::TimeTicks GetPlayoutTime(uint32 rtp_timestamp) const;

  // Schedule timing for the next cast message.
  void ScheduleNextCastMessage();

  // Schedule timing for the next RTCP report.
  void ScheduleNextRtcpReport();

  // Actually send the next cast message.
  void SendNextCastMessage();

  // Actually send the next RTCP report.
  void SendNextRtcpReport();

  // Receives a VideoFrame from |video_decoder_|, logs the event, and passes the
  // data on by running the given |callback|.  This method is static to ensure
  // it can be called after a VideoReceiver instance is destroyed.
  // DecodeEncodedVideoFrame() uses this as a callback for
  // VideoDecoder::DecodeFrame().
  static void EmitRawVideoFrame(
      const scoped_refptr<CastEnvironment>& cast_environment,
      const VideoFrameDecodedCallback& callback,
      uint32 frame_id,
      uint32 rtp_timestamp,
      const base::TimeTicks& playout_time,
      const scoped_refptr<VideoFrame>& video_frame,
      bool is_continuous);

  const scoped_refptr<CastEnvironment> cast_environment_;

  // Subscribes to raw events.
  // Processes raw audio events to be sent over to the cast sender via RTCP.
  ReceiverRtcpEventSubscriber event_subscriber_;

  // Configured audio codec.
  const transport::VideoCodec codec_;

  // The total amount of time between a frame's capture/recording on the sender
  // and its playback on the receiver (i.e., shown to a user).  This is fixed as
  // a value large enough to give the system sufficient time to encode,
  // transmit/retransmit, receive, decode, and render; given its run-time
  // environment (sender/receiver hardware performance, network conditions,
  // etc.).
  const base::TimeDelta target_playout_delay_;

  // Hack: This is used in logic that determines whether to skip frames.
  const base::TimeDelta expected_frame_duration_;

  // Set to false initially, then set to true after scheduling the periodic
  // sending of reports back to the sender.  Reports are first scheduled just
  // after receiving a first packet (since the first packet identifies the
  // sender for the remainder of the session).
  bool reports_are_scheduled_;

  // Assembles packets into frames, providing this receiver with complete,
  // decodable EncodedFrames.
  Framer framer_;

  // Decodes frames into media::VideoFrame images for playback.
  scoped_ptr<VideoDecoder> video_decoder_;

  // Manages sending/receiving of RTCP packets, including sender/receiver
  // reports.
  Rtcp rtcp_;

  // Decrypts encrypted frames.
  transport::TransportEncryptionHandler decryptor_;

  // Outstanding callbacks to run to deliver on client requests for frames.
  std::list<FrameEncodedCallback> frame_request_queue_;

  // True while there's an outstanding task to re-invoke
  // EmitAvailableEncodedFrames().
  bool is_waiting_for_consecutive_frame_;

  // This mapping allows us to log FRAME_ACK_SENT as a frame event. In addition
  // it allows the event to be transmitted via RTCP.
  RtpTimestamp frame_id_to_rtp_timestamp_[256];

  // Lip-sync values used to compute the playout time of each frame from its RTP
  // timestamp.  These are updated each time the first packet of a frame is
  // received.
  RtpTimestamp lip_sync_rtp_timestamp_;
  base::TimeTicks lip_sync_reference_time_;
  ClockDriftSmoother lip_sync_drift_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoReceiver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoReceiver);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_RECEIVER_VIDEO_RECEIVER_H_
