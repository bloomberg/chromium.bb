// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_
#define CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_

#include "base/id_map.h"
#include "base/time/default_tick_clock.h"
#include "chrome/common/cast_messages.h"
#include "content/public/browser/browser_message_filter.h"
#include "media/cast/cast_sender.h"
#include "media/cast/transport/cast_transport_sender.h"

namespace cast {

class CastTransportHostFilter : public content::BrowserMessageFilter {
 public:
  CastTransportHostFilter();
 private:
  virtual ~CastTransportHostFilter();

  void NotifyStatusChange(
      int32 channel_id,
      media::cast::transport::CastTransportStatus result);
  void ReceivedPacket(
      int32 channel_id,
      scoped_ptr<media::cast::transport::Packet> result);
  void ReceivedRtpStatistics(
      int32 channel_id,
      bool audio,
      const media::cast::transport::RtcpSenderInfo& sender_info,
      base::TimeTicks time_sent,
      uint32 rtp_timestamp);

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // Forwarding functions.
  void OnInsertCodedAudioFrame(
      int32 channel_id,
      const media::cast::transport::EncodedAudioFrame& audio_frame,
      base::TimeTicks recorded_time);
  void OnInsertCodedVideoFrame(
      int32 channel_id,
      const media::cast::transport::EncodedVideoFrame& video_frame,
      base::TimeTicks capture_time);
  void OnSendRtcpFromRtpSender(
      int32 channel_id,
      const media::cast::transport::SendRtcpFromRtpSenderData& data,
      const media::cast::transport::RtcpSenderInfo& sender_info,
      const media::cast::transport::RtcpDlrrReportBlock& dlrr,
      const media::cast::transport::RtcpSenderLogMessage& sender_log);
  void OnResendPackets(
      int32 channel_id,
      bool is_audio,
      const media::cast::MissingFramesAndPacketsMap& missing_packets);
  void OnNew(
      int32 channel_id,
      const media::cast::transport::CastTransportConfig& config);
  void OnDelete(int32 channel_id);

  IDMap<media::cast::transport::CastTransportSender, IDMapOwnPointer> id_map_;

  // Clock used by Cast transport.
  base::DefaultTickClock clock_;
  DISALLOW_COPY_AND_ASSIGN(CastTransportHostFilter);
};

}  // namespace cast

#endif  // CHROME_BROWSER_MEDIA_CAST_TRANSPORT_HOST_FILTER_H_
