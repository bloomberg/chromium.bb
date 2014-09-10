// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_
#define CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_

#include <map>

#include "base/message_loop/message_loop_proxy.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_sender.h"

// This implementation of the CastTransportSender interface
// communicates with the browser process over IPC and relays
// all calls to/from the transport sender to the browser process.
// The primary reason for this arrangement is to give the
// renderer less direct control over the UDP sockets.
class CastTransportSenderIPC
    : public media::cast::CastTransportSender {
 public:
  CastTransportSenderIPC(
      const net::IPEndPoint& remote_end_point,
      scoped_ptr<base::DictionaryValue> options,
      const media::cast::CastTransportStatusCallback& status_cb,
      const media::cast::BulkRawEventsCallback& raw_events_cb);

  virtual ~CastTransportSenderIPC();

  // media::cast::CastTransportSender implementation.
  virtual void InitializeAudio(
      const media::cast::CastTransportRtpConfig& config,
      const media::cast::RtcpCastMessageCallback& cast_message_cb,
      const media::cast::RtcpRttCallback& rtt_cb) OVERRIDE;
  virtual void InitializeVideo(
      const media::cast::CastTransportRtpConfig& config,
      const media::cast::RtcpCastMessageCallback& cast_message_cb,
      const media::cast::RtcpRttCallback& rtt_cb) OVERRIDE;
  virtual void InsertFrame(uint32 ssrc,
      const media::cast::EncodedFrame& frame) OVERRIDE;
  virtual void SendSenderReport(
      uint32 ssrc,
      base::TimeTicks current_time,
      uint32 current_time_as_rtp_timestamp) OVERRIDE;
  virtual void CancelSendingFrames(
      uint32 ssrc,
      const std::vector<uint32>& frame_ids) OVERRIDE;
  virtual void ResendFrameForKickstart(uint32 ssrc, uint32 frame_id) OVERRIDE;

  void OnNotifyStatusChange(
      media::cast::CastTransportStatus status);
  void OnRawEvents(const std::vector<media::cast::PacketEvent>& packet_events,
                   const std::vector<media::cast::FrameEvent>& frame_events);
  void OnRtt(uint32 ssrc, base::TimeDelta rtt);
  void OnRtcpCastMessage(uint32 ssrc,
                         const media::cast::RtcpCastMessage& cast_message);

 private:
  struct ClientCallbacks {
    ClientCallbacks();
    ~ClientCallbacks();

    media::cast::RtcpCastMessageCallback cast_message_cb;
    media::cast::RtcpRttCallback rtt_cb;
  };

  void Send(IPC::Message* message);

  int32 channel_id_;
  media::cast::PacketReceiverCallback packet_callback_;
  media::cast::CastTransportStatusCallback status_callback_;
  media::cast::BulkRawEventsCallback raw_events_callback_;
  typedef std::map<uint32, ClientCallbacks> ClientMap;
  ClientMap clients_;

  DISALLOW_COPY_AND_ASSIGN(CastTransportSenderIPC);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_
