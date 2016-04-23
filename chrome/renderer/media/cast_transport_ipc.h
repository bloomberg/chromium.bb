// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_TRANSPORT_IPC_H_
#define CHROME_RENDERER_MEDIA_CAST_TRANSPORT_IPC_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/thread_task_runner_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport.h"

// This implementation of the CastTransport interface communicates with the
// browser process over IPC and relays all calls to/from the cast transport to
// the browser process. The primary reason for this arrangement is to give the
// renderer less direct control over the UDP sockets.
class CastTransportIPC : public media::cast::CastTransport {
 public:
  CastTransportIPC(const net::IPEndPoint& local_end_point,
                   const net::IPEndPoint& remote_end_point,
                   std::unique_ptr<base::DictionaryValue> options,
                   const media::cast::PacketReceiverCallback& packet_callback,
                   const media::cast::CastTransportStatusCallback& status_cb,
                   const media::cast::BulkRawEventsCallback& raw_events_cb);

  ~CastTransportIPC() override;

  // media::cast::CastTransport implementation.
  void InitializeAudio(
      const media::cast::CastTransportRtpConfig& config,
      const media::cast::RtcpCastMessageCallback& cast_message_cb,
      const media::cast::RtcpRttCallback& rtt_cb,
      const media::cast::RtcpPliCallback& pli_cb) override;
  void InitializeVideo(
      const media::cast::CastTransportRtpConfig& config,
      const media::cast::RtcpCastMessageCallback& cast_message_cb,
      const media::cast::RtcpRttCallback& rtt_cb,
      const media::cast::RtcpPliCallback& pli_cb) override;
  void InsertFrame(uint32_t ssrc,
                   const media::cast::EncodedFrame& frame) override;
  void SendSenderReport(
      uint32_t ssrc,
      base::TimeTicks current_time,
      media::cast::RtpTimeTicks current_time_as_rtp_timestamp) override;
  void CancelSendingFrames(
      uint32_t ssrc,
      const std::vector<media::cast::FrameId>& frame_ids) override;
  void ResendFrameForKickstart(uint32_t ssrc,
                               media::cast::FrameId frame_id) override;
  void AddValidRtpReceiver(uint32_t rtp_sender_ssrc,
                           uint32_t rtp_receiver_ssrc) override;
  void InitializeRtpReceiverRtcpBuilder(
      uint32_t rtp_receiver_ssrc,
      const media::cast::RtcpTimeData& time_data) override;
  void AddCastFeedback(const media::cast::RtcpCastMessage& cast_message,
                       base::TimeDelta target_delay) override;
  void AddPli(const media::cast::RtcpPliMessage& pli_message) override;
  void AddRtcpEvents(const media::cast::ReceiverRtcpEventSubscriber::RtcpEvents&
                         rtcp_events) override;
  void AddRtpReceiverReport(
      const media::cast::RtcpReportBlock& rtp_receiver_report_block) override;
  void SendRtcpFromRtpReceiver() override;
  void SetOptions(const base::DictionaryValue& options) final {}
  void OnNotifyStatusChange(media::cast::CastTransportStatus status);
  void OnRawEvents(const std::vector<media::cast::PacketEvent>& packet_events,
                   const std::vector<media::cast::FrameEvent>& frame_events);
  void OnRtt(uint32_t rtp_sender_ssrc, base::TimeDelta rtt);
  void OnRtcpCastMessage(uint32_t rtp_sender_ssrc,
                         const media::cast::RtcpCastMessage& cast_message);
  void OnReceivedPli(uint32_t rtp_sender_ssrc);
  void OnReceivedPacket(const media::cast::Packet& packet);

 private:
  struct ClientCallbacks {
    ClientCallbacks();
    ClientCallbacks(const ClientCallbacks& other);
    ~ClientCallbacks();

    media::cast::RtcpCastMessageCallback cast_message_cb;
    media::cast::RtcpRttCallback rtt_cb;
    media::cast::RtcpPliCallback pli_cb;
  };

  void Send(IPC::Message* message);

  int32_t channel_id_;
  media::cast::PacketReceiverCallback packet_callback_;
  media::cast::CastTransportStatusCallback status_callback_;
  media::cast::BulkRawEventsCallback raw_events_callback_;
  typedef std::map<uint32_t, ClientCallbacks> ClientMap;
  ClientMap clients_;

  DISALLOW_COPY_AND_ASSIGN(CastTransportIPC);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_TRANSPORT_IPC_H_
