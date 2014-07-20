// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class maintains a send transport for audio and video in a Cast
// Streaming session.
// Audio, video frames and RTCP messages are submitted to this object
// and then packetized and paced to the underlying UDP socket.
//
// The hierarchy of send transport in a Cast Streaming session:
//
// CastTransportSender              RTP                      RTCP
// ------------------------------------------------------------------
//                      TransportEncryptionHandler (A/V)
//                      RtpSender (A/V)                   Rtcp (A/V)
//                                      PacedSender (Shared)
//                                      UdpTransport (Shared)
//
// There are objects of TransportEncryptionHandler, RtpSender and Rtcp
// for each audio and video stream.
// PacedSender and UdpTransport are shared between all RTP and RTCP
// streams.

#ifndef MEDIA_CAST_NET_CAST_TRANSPORT_IMPL_H_
#define MEDIA_CAST_NET_CAST_TRANSPORT_IMPL_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/cast/common/transport_encryption_handler.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_sender.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtcp/rtcp.h"
#include "media/cast/net/rtp/rtp_sender.h"

namespace media {
namespace cast {

class UdpTransport;

class CastTransportSenderImpl : public CastTransportSender {
 public:
  // |external_transport| is only used for testing.
  // |raw_events_callback|: Raw events will be returned on this callback
  // which will be invoked every |raw_events_callback_interval|.
  // This can be a null callback, i.e. if user is not interested in raw events.
  // |raw_events_callback_interval|: This can be |base::TimeDelta()| if
  // |raw_events_callback| is a null callback.
  CastTransportSenderImpl(
      net::NetLog* net_log,
      base::TickClock* clock,
      const net::IPEndPoint& remote_end_point,
      const CastTransportStatusCallback& status_callback,
      const BulkRawEventsCallback& raw_events_callback,
      base::TimeDelta raw_events_callback_interval,
      const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner,
      PacketSender* external_transport);

  virtual ~CastTransportSenderImpl();

  virtual void InitializeAudio(const CastTransportRtpConfig& config,
                               const RtcpCastMessageCallback& cast_message_cb,
                               const RtcpRttCallback& rtt_cb) OVERRIDE;
  virtual void InitializeVideo(const CastTransportRtpConfig& config,
                               const RtcpCastMessageCallback& cast_message_cb,
                               const RtcpRttCallback& rtt_cb) OVERRIDE;
  virtual void InsertCodedAudioFrame(const EncodedFrame& audio_frame) OVERRIDE;
  virtual void InsertCodedVideoFrame(const EncodedFrame& video_frame) OVERRIDE;

  virtual void SendSenderReport(
      uint32 ssrc,
      base::TimeTicks current_time,
      uint32 current_time_as_rtp_timestamp) OVERRIDE;

  virtual void ResendPackets(bool is_audio,
                             const MissingFramesAndPacketsMap& missing_packets,
                             bool cancel_rtx_if_not_in_list,
                             base::TimeDelta dedupe_window)
      OVERRIDE;

  virtual PacketReceiverCallback PacketReceiverForTesting() OVERRIDE;

 private:
  // If |raw_events_callback_| is non-null, calls it with events collected
  // by |event_subscriber_| since last call.
  void SendRawEvents();

  // Start receiving packets.
  void StartReceiving();

  // Called when a packet is received.
  void OnReceivedPacket(scoped_ptr<Packet> packet);

  // Called when a log message is received.
  void OnReceivedLogMessage(EventMediaType media_type,
                            const RtcpReceiverLogMessage& log);

  base::TickClock* clock_;  // Not owned by this class.
  CastTransportStatusCallback status_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> transport_task_runner_;

  LoggingImpl logging_;

  // Interface to a UDP socket.
  scoped_ptr<UdpTransport> transport_;

  // Packet sender that performs pacing.
  PacedSender pacer_;

  // Packetizer for audio and video frames.
  scoped_ptr<RtpSender> audio_sender_;
  scoped_ptr<RtpSender> video_sender_;

  // Maintains RTCP session for audio and video.
  scoped_ptr<Rtcp> audio_rtcp_session_;
  scoped_ptr<Rtcp> video_rtcp_session_;

  // Encrypts data in EncodedFrames before they are sent.  Note that it's
  // important for the encryption to happen here, in code that would execute in
  // the main browser process, for security reasons.  This helps to mitigate
  // the damage that could be caused by a compromised renderer process.
  TransportEncryptionHandler audio_encryptor_;
  TransportEncryptionHandler video_encryptor_;

  // This is non-null iff |raw_events_callback_| is non-null.
  scoped_ptr<SimpleEventSubscriber> event_subscriber_;

  BulkRawEventsCallback raw_events_callback_;
  base::TimeDelta raw_events_callback_interval_;

  base::WeakPtrFactory<CastTransportSenderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastTransportSenderImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_CAST_TRANSPORT_IMPL_H_
