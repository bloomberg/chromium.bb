// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_AUDIO_RECEIVER_AUDIO_RECEIVER_H_
#define MEDIA_CAST_AUDIO_RECEIVER_AUDIO_RECEIVER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
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
namespace cast {

class AudioDecoder;

// AudioReceiver receives packets out-of-order while clients make requests for
// complete frames in-order.  (A frame consists of one or more packets.)
//
// AudioReceiver also includes logic for computing the playout time for each
// frame, accounting for a constant targeted playout delay.  The purpose of the
// playout delay is to provide a fixed window of time between the capture event
// on the sender and the playout on the receiver.  This is important because
// each step of the pipeline (i.e., encode frame, then transmit/retransmit from
// the sender, then receive and re-order packets on the receiver, then decode
// frame) can vary in duration and is typically very hard to predict.
// Heuristics will determine when the targeted playout delay is insufficient in
// the current environment; and the receiver can then increase the playout
// delay, notifying the sender, to account for the extra variance.
// TODO(miu): Make the last sentence true.  http://crbug.com/360111
//
// Two types of frames can be requested: 1) A frame of decoded audio data; or 2)
// a frame of still-encoded audio data, to be passed into an external audio
// decoder.  Each request for a frame includes a callback which AudioReceiver
// guarantees will be called at some point in the future.  Clients should
// generally limit the number of outstanding requests (perhaps to just one or
// two).  When AudioReceiver is destroyed, any outstanding requests will be
// immediately invoked with a NULL frame.
//
// This class is not thread safe.  Should only be called from the Main cast
// thread.
class AudioReceiver : public RtpReceiver,
                      public RtpPayloadFeedback,
                      public base::NonThreadSafe,
                      public base::SupportsWeakPtr<AudioReceiver> {
 public:
  AudioReceiver(scoped_refptr<CastEnvironment> cast_environment,
                const AudioReceiverConfig& audio_config,
                transport::PacedPacketSender* const packet_sender);

  virtual ~AudioReceiver();

  // Request a decoded audio frame.  The audio signal data returned in the
  // callback will have the sampling rate and number of channels as requested in
  // the configuration that was passed to the ctor.
  //
  // The given |callback| is guaranteed to be run at some point in the future,
  // even if to respond with NULL at shutdown time.
  void GetRawAudioFrame(const AudioFrameDecodedCallback& callback);

  // Request an encoded audio frame.
  //
  // The given |callback| is guaranteed to be run at some point in the future,
  // even if to respond with NULL at shutdown time.
  void GetEncodedAudioFrame(const AudioFrameEncodedCallback& callback);

  // Deliver another packet, possibly a duplicate, and possibly out-of-order.
  void IncomingPacket(scoped_ptr<Packet> packet);

  // Update target audio delay used to compute the playout time. Rtcp
  // will also be updated (will be included in all outgoing reports).
  void SetTargetDelay(base::TimeDelta target_delay);

 protected:
  friend class AudioReceiverTest;  // Invokes OnReceivedPayloadData().

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

  // Feeds an EncodedAudioFrame into |audio_decoder_|.  GetRawAudioFrame() uses
  // this as a callback for GetEncodedAudioFrame().
  void DecodeEncodedAudioFrame(
      const AudioFrameDecodedCallback& callback,
      scoped_ptr<transport::EncodedAudioFrame> encoded_frame,
      const base::TimeTicks& playout_time);

  // Return the playout time based on the current time and rtp timestamp.
  base::TimeTicks GetPlayoutTime(base::TimeTicks now, uint32 rtp_timestamp);

  void InitializeTimers();

  // Schedule the next RTCP report.
  void ScheduleNextRtcpReport();

  // Actually send the next RTCP report.
  void SendNextRtcpReport();

  // Schedule timing for the next cast message.
  void ScheduleNextCastMessage();

  // Actually send the next cast message.
  void SendNextCastMessage();

  // Receives an AudioBus from |audio_decoder_|, logs the event, and passes the
  // data on by running the given |callback|.  This method is static to ensure
  // it can be called after an AudioReceiver instance is destroyed.
  // DecodeEncodedAudioFrame() uses this as a callback for
  // AudioDecoder::DecodeFrame().
  static void EmitRawAudioFrame(
      const scoped_refptr<CastEnvironment>& cast_environment,
      const AudioFrameDecodedCallback& callback,
      uint32 frame_id,
      uint32 rtp_timestamp,
      const base::TimeTicks& playout_time,
      scoped_ptr<AudioBus> audio_bus,
      bool is_continuous);

  const scoped_refptr<CastEnvironment> cast_environment_;

  // Subscribes to raw events.
  // Processes raw audio events to be sent over to the cast sender via RTCP.
  ReceiverRtcpEventSubscriber event_subscriber_;

  const transport::AudioCodec codec_;
  const int frequency_;
  base::TimeDelta target_delay_delta_;
  Framer framer_;
  scoped_ptr<AudioDecoder> audio_decoder_;
  Rtcp rtcp_;
  base::TimeDelta time_offset_;
  base::TimeTicks time_first_incoming_packet_;
  uint32 first_incoming_rtp_timestamp_;
  transport::TransportEncryptionHandler decryptor_;

  // Outstanding callbacks to run to deliver on client requests for frames.
  std::list<AudioFrameEncodedCallback> frame_request_queue_;

  // True while there's an outstanding task to re-invoke
  // EmitAvailableEncodedFrames().
  bool is_waiting_for_consecutive_frame_;

  // This mapping allows us to log kAudioAckSent as a frame event. In addition
  // it allows the event to be transmitted via RTCP.
  RtpTimestamp frame_id_to_rtp_timestamp_[256];

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<AudioReceiver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioReceiver);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_AUDIO_RECEIVER_AUDIO_RECEIVER_H_
