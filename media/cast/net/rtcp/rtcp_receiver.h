// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTCP_RTCP_RECEIVER_H_
#define MEDIA_CAST_RTCP_RTCP_RECEIVER_H_

#include <queue>

#include "base/containers/hash_tables.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/rtcp/rtcp.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/net/rtcp/rtcp_utility.h"

namespace media {
namespace cast {

// Interface for receiving RTCP messages.
class RtcpMessageHandler {
 public:
  virtual void OnReceivedSenderReport(
      const RtcpSenderInfo& remote_sender_info) = 0;

  virtual void OnReceiverReferenceTimeReport(
      const RtcpReceiverReferenceTimeReport& remote_time_report) = 0;

  virtual void OnReceivedReceiverLog(
      const RtcpReceiverLogMessage& receiver_log) = 0;

  virtual void OnReceivedDelaySinceLastReport(
      uint32 last_report,
      uint32 delay_since_last_report) = 0;

  virtual void OnReceivedCastFeedback(
      const RtcpCastMessage& cast_message) = 0;

  virtual ~RtcpMessageHandler() {}
};

class RtcpReceiver {
 public:
  RtcpReceiver(RtcpMessageHandler* handler, uint32 local_ssrc);
  virtual ~RtcpReceiver();

  static bool IsRtcpPacket(const uint8* rtcp_buffer, size_t length);

  static uint32 GetSsrcOfSender(const uint8* rtcp_buffer, size_t length);

  void SetRemoteSSRC(uint32 ssrc);

  // Set the history size to record Cast receiver events. Event history is
  // used to remove duplicates. The history has no more than |size| events.
  void SetCastReceiverEventHistorySize(size_t size);

  void IncomingRtcpPacket(RtcpParser* rtcp_parser);

 private:
  void HandleSenderReport(RtcpParser* rtcp_parser);

  void HandleReceiverReport(RtcpParser* rtcp_parser);

  void HandleReportBlock(const RtcpField* rtcp_field, uint32 remote_ssrc);

  void HandleXr(RtcpParser* rtcp_parser);
  void HandleRrtr(RtcpParser* rtcp_parser, uint32 remote_ssrc);
  void HandleDlrr(RtcpParser* rtcp_parser);

  void HandlePayloadSpecificApp(RtcpParser* rtcp_parser);
  void HandlePayloadSpecificCastItem(RtcpParser* rtcp_parser);
  void HandlePayloadSpecificCastNackItem(
      const RtcpField* rtcp_field,
      MissingFramesAndPacketsMap* missing_frames_and_packets);

  void HandleApplicationSpecificCastReceiverLog(RtcpParser* rtcp_parser);
  void HandleApplicationSpecificCastReceiverEventLog(
      uint32 frame_rtp_timestamp,
      RtcpParser* rtcp_parser,
      RtcpReceiverEventLogMessages* event_log_messages);

  const uint32 ssrc_;
  uint32 remote_ssrc_;

  // Not owned by this class.
  RtcpMessageHandler* const handler_;

  FrameIdWrapHelper ack_frame_id_wrap_helper_;

  // Maintains a history of receiver events.
  size_t receiver_event_history_size_;
  typedef std::pair<uint64, uint64> ReceiverEventKey;
  base::hash_set<ReceiverEventKey> receiver_event_key_set_;
  std::queue<ReceiverEventKey> receiver_event_key_queue_;

  DISALLOW_COPY_AND_ASSIGN(RtcpReceiver);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTCP_RTCP_RECEIVER_H_
