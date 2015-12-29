// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_
#define CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/thread_task_runner_handle.h"
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
      const net::IPEndPoint& local_end_point,
      const net::IPEndPoint& remote_end_point,
      scoped_ptr<base::DictionaryValue> options,
      const media::cast::PacketReceiverCallback& packet_callback,
      const media::cast::CastTransportStatusCallback& status_cb,
      const media::cast::BulkRawEventsCallback& raw_events_cb);

  ~CastTransportSenderIPC() override;

  // media::cast::CastTransportSender implementation.
  void InitializeAudio(
      const media::cast::CastTransportRtpConfig& config,
      const media::cast::RtcpCastMessageCallback& cast_message_cb,
      const media::cast::RtcpRttCallback& rtt_cb) override;
  void InitializeVideo(
      const media::cast::CastTransportRtpConfig& config,
      const media::cast::RtcpCastMessageCallback& cast_message_cb,
      const media::cast::RtcpRttCallback& rtt_cb) override;
  void InsertFrame(uint32_t ssrc,
                   const media::cast::EncodedFrame& frame) override;
  void SendSenderReport(
      uint32_t ssrc,
      base::TimeTicks current_time,
      media::cast::RtpTimeTicks current_time_as_rtp_timestamp) override;
  void CancelSendingFrames(uint32_t ssrc,
                           const std::vector<uint32_t>& frame_ids) override;
  void ResendFrameForKickstart(uint32_t ssrc, uint32_t frame_id) override;
  void AddValidSsrc(uint32_t ssrc) override;
  void SendRtcpFromRtpReceiver(
      uint32_t ssrc,
      uint32_t sender_ssrc,
      const media::cast::RtcpTimeData& time_data,
      const media::cast::RtcpCastMessage* cast_message,
      base::TimeDelta target_delay,
      const media::cast::ReceiverRtcpEventSubscriber::RtcpEvents* rtcp_events,
      const media::cast::RtpReceiverStatistics* rtp_receiver_statistics)
      override;

  void OnNotifyStatusChange(
      media::cast::CastTransportStatus status);
  void OnRawEvents(const std::vector<media::cast::PacketEvent>& packet_events,
                   const std::vector<media::cast::FrameEvent>& frame_events);
  void OnRtt(uint32_t ssrc, base::TimeDelta rtt);
  void OnRtcpCastMessage(uint32_t ssrc,
                         const media::cast::RtcpCastMessage& cast_message);
  void OnReceivedPacket(const media::cast::Packet& packet);

 private:
  struct ClientCallbacks {
    ClientCallbacks();
    ~ClientCallbacks();

    media::cast::RtcpCastMessageCallback cast_message_cb;
    media::cast::RtcpRttCallback rtt_cb;
  };

  void Send(IPC::Message* message);

  int32_t channel_id_;
  media::cast::PacketReceiverCallback packet_callback_;
  media::cast::CastTransportStatusCallback status_callback_;
  media::cast::BulkRawEventsCallback raw_events_callback_;
  typedef std::map<uint32_t, ClientCallbacks> ClientMap;
  ClientMap clients_;

  DISALLOW_COPY_AND_ASSIGN(CastTransportSenderIPC);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_TRANSPORT_SENDER_IPC_H_
